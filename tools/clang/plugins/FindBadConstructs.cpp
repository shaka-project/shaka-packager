// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a bunch of recurring problems in the Chromium C++ code.
//
// Checks that are implemented:
// - Constructors/Destructors should not be inlined if they are of a complex
//   class type.
// - Missing "virtual" keywords on methods that should be virtual.
// - Non-annotated overriding virtual methods.
// - Virtual methods with nonempty implementations in their headers.
// - Classes that derive from base::RefCounted / base::RefCountedThreadSafe
//   should have protected or private destructors.

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/AST.h"
#include "clang/AST/Attr.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/raw_ostream.h"

#include "ChromeClassTester.h"

using namespace clang;

namespace {

const char kMethodRequiresOverride[] =
    "[chromium-style] Overriding method must be marked with OVERRIDE.";
const char kMethodRequiresVirtual[] =
    "[chromium-style] Overriding method must have \"virtual\" keyword.";
const char kNoExplicitDtor[] =
    "[chromium-style] Classes that are ref-counted should have explicit "
    "destructors that are declared protected or private.";
const char kPublicDtor[] =
    "[chromium-style] Classes that are ref-counted should have "
    "destructors that are declared protected or private.";
const char kProtectedNonVirtualDtor[] =
    "[chromium-style] Classes that are ref-counted and have non-private "
    "destructors should declare their destructor virtual.";
const char kNoteInheritance[] =
    "[chromium-style] %0 inherits from %1 here";
const char kNoteImplicitDtor[] =
    "[chromium-style] No explicit destructor for %0 defined";
const char kNotePublicDtor[] =
    "[chromium-style] Public destructor declared here";
const char kNoteProtectedNonVirtualDtor[] =
    "[chromium-style] Protected non-virtual destructor declared here";

bool TypeHasNonTrivialDtor(const Type* type) {
  if (const CXXRecordDecl* cxx_r = type->getPointeeCXXRecordDecl())
    return cxx_r->hasTrivialDestructor();

  return false;
}

// Returns the underlying Type for |type| by expanding typedefs and removing
// any namespace qualifiers. This is similar to desugaring, except that for
// ElaboratedTypes, desugar will unwrap too much.
const Type* UnwrapType(const Type* type) {
  if (const ElaboratedType* elaborated = dyn_cast<ElaboratedType>(type))
    return UnwrapType(elaborated->getNamedType().getTypePtr());
  if (const TypedefType* typedefed = dyn_cast<TypedefType>(type))
    return UnwrapType(typedefed->desugar().getTypePtr());
  return type;
}

// Searches for constructs that we know we don't want in the Chromium code base.
class FindBadConstructsConsumer : public ChromeClassTester {
 public:
  FindBadConstructsConsumer(CompilerInstance& instance,
                            bool check_base_classes,
                            bool check_virtuals_in_implementations,
                            bool check_url_directory)
      : ChromeClassTester(instance, check_url_directory),
        check_base_classes_(check_base_classes),
        check_virtuals_in_implementations_(check_virtuals_in_implementations) {
    // Register warning/error messages.
    diag_method_requires_override_ = diagnostic().getCustomDiagID(
        getErrorLevel(), kMethodRequiresOverride);
    diag_method_requires_virtual_ = diagnostic().getCustomDiagID(
        getErrorLevel(), kMethodRequiresVirtual);
    diag_no_explicit_dtor_ = diagnostic().getCustomDiagID(
        getErrorLevel(), kNoExplicitDtor);
    diag_public_dtor_ = diagnostic().getCustomDiagID(
        getErrorLevel(), kPublicDtor);
    diag_protected_non_virtual_dtor_ = diagnostic().getCustomDiagID(
        getErrorLevel(), kProtectedNonVirtualDtor);

    // Registers notes to make it easier to interpret warnings.
    diag_note_inheritance_ = diagnostic().getCustomDiagID(
        DiagnosticsEngine::Note, kNoteInheritance);
    diag_note_implicit_dtor_ = diagnostic().getCustomDiagID(
        DiagnosticsEngine::Note, kNoteImplicitDtor);
    diag_note_public_dtor_ = diagnostic().getCustomDiagID(
        DiagnosticsEngine::Note, kNotePublicDtor);
    diag_note_protected_non_virtual_dtor_ = diagnostic().getCustomDiagID(
        DiagnosticsEngine::Note, kNoteProtectedNonVirtualDtor);
  }

  virtual void CheckChromeClass(SourceLocation record_location,
                                CXXRecordDecl* record) {
    bool implementation_file = InImplementationFile(record_location);

    if (!implementation_file) {
      // Only check for "heavy" constructors/destructors in header files;
      // within implementation files, there is no performance cost.
      CheckCtorDtorWeight(record_location, record);
    }

    if (!implementation_file || check_virtuals_in_implementations_) {
      bool warn_on_inline_bodies = !implementation_file;

      // Check that all virtual methods are marked accordingly with both
      // virtual and OVERRIDE.
      CheckVirtualMethods(record_location, record, warn_on_inline_bodies);
    }

    CheckRefCountedDtors(record_location, record);
  }

 private:
  // The type of problematic ref-counting pattern that was encountered.
  enum RefcountIssue {
    None,
    ImplicitDestructor,
    PublicDestructor
  };

  bool check_base_classes_;
  bool check_virtuals_in_implementations_;

  unsigned diag_method_requires_override_;
  unsigned diag_method_requires_virtual_;
  unsigned diag_no_explicit_dtor_;
  unsigned diag_public_dtor_;
  unsigned diag_protected_non_virtual_dtor_;
  unsigned diag_note_inheritance_;
  unsigned diag_note_implicit_dtor_;
  unsigned diag_note_public_dtor_;
  unsigned diag_note_protected_non_virtual_dtor_;

  // Prints errors if the constructor/destructor weight is too heavy.
  void CheckCtorDtorWeight(SourceLocation record_location,
                           CXXRecordDecl* record) {
    // We don't handle anonymous structs. If this record doesn't have a
    // name, it's of the form:
    //
    // struct {
    //   ...
    // } name_;
    if (record->getIdentifier() == NULL)
      return;

    // Count the number of templated base classes as a feature of whether the
    // destructor can be inlined.
    int templated_base_classes = 0;
    for (CXXRecordDecl::base_class_const_iterator it = record->bases_begin();
         it != record->bases_end(); ++it) {
      if (it->getTypeSourceInfo()->getTypeLoc().getTypeLocClass() ==
          TypeLoc::TemplateSpecialization) {
        ++templated_base_classes;
      }
    }

    // Count the number of trivial and non-trivial member variables.
    int trivial_member = 0;
    int non_trivial_member = 0;
    int templated_non_trivial_member = 0;
    for (RecordDecl::field_iterator it = record->field_begin();
         it != record->field_end(); ++it) {
      CountType(it->getType().getTypePtr(),
                &trivial_member,
                &non_trivial_member,
                &templated_non_trivial_member);
    }

    // Check to see if we need to ban inlined/synthesized constructors. Note
    // that the cutoffs here are kind of arbitrary. Scores over 10 break.
    int dtor_score = 0;
    // Deriving from a templated base class shouldn't be enough to trigger
    // the ctor warning, but if you do *anything* else, it should.
    //
    // TODO(erg): This is motivated by templated base classes that don't have
    // any data members. Somehow detect when templated base classes have data
    // members and treat them differently.
    dtor_score += templated_base_classes * 9;
    // Instantiating a template is an insta-hit.
    dtor_score += templated_non_trivial_member * 10;
    // The fourth normal class member should trigger the warning.
    dtor_score += non_trivial_member * 3;

    int ctor_score = dtor_score;
    // You should be able to have 9 ints before we warn you.
    ctor_score += trivial_member;

    if (ctor_score >= 10) {
      if (!record->hasUserDeclaredConstructor()) {
        emitWarning(record_location,
                    "Complex class/struct needs an explicit out-of-line "
                    "constructor.");
      } else {
        // Iterate across all the constructors in this file and yell if we
        // find one that tries to be inline.
        for (CXXRecordDecl::ctor_iterator it = record->ctor_begin();
             it != record->ctor_end(); ++it) {
          if (it->hasInlineBody()) {
            if (it->isCopyConstructor() &&
                !record->hasUserDeclaredCopyConstructor()) {
              emitWarning(record_location,
                          "Complex class/struct needs an explicit out-of-line "
                          "copy constructor.");
            } else {
              emitWarning(it->getInnerLocStart(),
                          "Complex constructor has an inlined body.");
            }
          }
        }
      }
    }

    // The destructor side is equivalent except that we don't check for
    // trivial members; 20 ints don't need a destructor.
    if (dtor_score >= 10 && !record->hasTrivialDestructor()) {
      if (!record->hasUserDeclaredDestructor()) {
        emitWarning(
            record_location,
            "Complex class/struct needs an explicit out-of-line "
            "destructor.");
      } else if (CXXDestructorDecl* dtor = record->getDestructor()) {
        if (dtor->hasInlineBody()) {
          emitWarning(dtor->getInnerLocStart(),
                      "Complex destructor has an inline body.");
        }
      }
    }
  }

  void CheckVirtualMethod(const CXXMethodDecl* method,
                          bool warn_on_inline_bodies) {
    if (!method->isVirtual())
      return;

    if (!method->isVirtualAsWritten()) {
      SourceLocation loc = method->getTypeSpecStartLoc();
      if (isa<CXXDestructorDecl>(method))
        loc = method->getInnerLocStart();
      SourceManager& manager = instance().getSourceManager();
      FullSourceLoc full_loc(loc, manager);
      SourceLocation spelling_loc = manager.getSpellingLoc(loc);
      diagnostic().Report(full_loc, diag_method_requires_virtual_)
          << FixItHint::CreateInsertion(spelling_loc, "virtual ");
    }

    // Virtual methods should not have inline definitions beyond "{}". This
    // only matters for header files.
    if (warn_on_inline_bodies && method->hasBody() &&
        method->hasInlineBody()) {
      if (CompoundStmt* cs = dyn_cast<CompoundStmt>(method->getBody())) {
        if (cs->size()) {
          emitWarning(
              cs->getLBracLoc(),
              "virtual methods with non-empty bodies shouldn't be "
              "declared inline.");
        }
      }
    }
  }

  bool InTestingNamespace(const Decl* record) {
    return GetNamespace(record).find("testing") != std::string::npos;
  }

  bool IsMethodInBannedOrTestingNamespace(const CXXMethodDecl* method) {
    if (InBannedNamespace(method))
      return true;
    for (CXXMethodDecl::method_iterator i = method->begin_overridden_methods();
         i != method->end_overridden_methods();
         ++i) {
      const CXXMethodDecl* overridden = *i;
      if (IsMethodInBannedOrTestingNamespace(overridden) ||
          InTestingNamespace(overridden)) {
        return true;
      }
    }

    return false;
  }

  void CheckOverriddenMethod(const CXXMethodDecl* method) {
    if (!method->size_overridden_methods() || method->getAttr<OverrideAttr>())
      return;

    if (isa<CXXDestructorDecl>(method) || method->isPure())
      return;

    if (IsMethodInBannedOrTestingNamespace(method))
      return;

    SourceManager& manager = instance().getSourceManager();
    SourceRange type_info_range =
        method->getTypeSourceInfo()->getTypeLoc().getSourceRange();
    FullSourceLoc loc(type_info_range.getBegin(), manager);

    // Build the FixIt insertion point after the end of the method definition,
    // including any const-qualifiers and attributes, and before the opening
    // of the l-curly-brace (if inline) or the semi-color (if a declaration).
    SourceLocation spelling_end =
        manager.getSpellingLoc(type_info_range.getEnd());
    if (spelling_end.isValid()) {
      SourceLocation token_end = Lexer::getLocForEndOfToken(
          spelling_end, 0, manager, LangOptions());
      diagnostic().Report(token_end, diag_method_requires_override_)
          << FixItHint::CreateInsertion(token_end, " OVERRIDE");
    } else {
      diagnostic().Report(loc, diag_method_requires_override_);
    }
  }

  // Makes sure there is a "virtual" keyword on virtual methods.
  //
  // Gmock objects trigger these for each MOCK_BLAH() macro used. So we have a
  // trick to get around that. If a class has member variables whose types are
  // in the "testing" namespace (which is how gmock works behind the scenes),
  // there's a really high chance we won't care about these errors
  void CheckVirtualMethods(SourceLocation record_location,
                           CXXRecordDecl* record,
                           bool warn_on_inline_bodies) {
    for (CXXRecordDecl::field_iterator it = record->field_begin();
         it != record->field_end(); ++it) {
      CXXRecordDecl* record_type =
          it->getTypeSourceInfo()->getTypeLoc().getTypePtr()->
          getAsCXXRecordDecl();
      if (record_type) {
        if (InTestingNamespace(record_type)) {
          return;
        }
      }
    }

    for (CXXRecordDecl::method_iterator it = record->method_begin();
         it != record->method_end(); ++it) {
      if (it->isCopyAssignmentOperator() || isa<CXXConstructorDecl>(*it)) {
        // Ignore constructors and assignment operators.
      } else if (isa<CXXDestructorDecl>(*it) &&
          !record->hasUserDeclaredDestructor()) {
        // Ignore non-user-declared destructors.
      } else {
        CheckVirtualMethod(*it, warn_on_inline_bodies);
        CheckOverriddenMethod(*it);
      }
    }
  }

  void CountType(const Type* type,
                 int* trivial_member,
                 int* non_trivial_member,
                 int* templated_non_trivial_member) {
    switch (type->getTypeClass()) {
      case Type::Record: {
        // Simplifying; the whole class isn't trivial if the dtor is, but
        // we use this as a signal about complexity.
        if (TypeHasNonTrivialDtor(type))
          (*trivial_member)++;
        else
          (*non_trivial_member)++;
        break;
      }
      case Type::TemplateSpecialization: {
        TemplateName name =
            dyn_cast<TemplateSpecializationType>(type)->getTemplateName();
        bool whitelisted_template = false;

        // HACK: I'm at a loss about how to get the syntax checker to get
        // whether a template is exterened or not. For the first pass here,
        // just do retarded string comparisons.
        if (TemplateDecl* decl = name.getAsTemplateDecl()) {
          std::string base_name = decl->getNameAsString();
          if (base_name == "basic_string")
            whitelisted_template = true;
        }

        if (whitelisted_template)
          (*non_trivial_member)++;
        else
          (*templated_non_trivial_member)++;
        break;
      }
      case Type::Elaborated: {
        CountType(
            dyn_cast<ElaboratedType>(type)->getNamedType().getTypePtr(),
            trivial_member, non_trivial_member, templated_non_trivial_member);
        break;
      }
      case Type::Typedef: {
        while (const TypedefType* TT = dyn_cast<TypedefType>(type)) {
          type = TT->getDecl()->getUnderlyingType().getTypePtr();
        }
        CountType(type, trivial_member, non_trivial_member,
                  templated_non_trivial_member);
        break;
      }
      default: {
        // Stupid assumption: anything we see that isn't the above is one of
        // the 20 integer types.
        (*trivial_member)++;
        break;
      }
    }
  }

  // Check |record| for issues that are problematic for ref-counted types.
  // Note that |record| may not be a ref-counted type, but a base class for
  // a type that is.
  // If there are issues, update |loc| with the SourceLocation of the issue
  // and returns appropriately, or returns None if there are no issues.
  static RefcountIssue CheckRecordForRefcountIssue(
      const CXXRecordDecl* record,
      SourceLocation &loc) {
    if (!record->hasUserDeclaredDestructor()) {
      loc = record->getLocation();
      return ImplicitDestructor;
    }

    if (CXXDestructorDecl* dtor = record->getDestructor()) {
      if (dtor->getAccess() == AS_public) {
        loc = dtor->getInnerLocStart();
        return PublicDestructor;
      }
    }

    return None;
  }

  // Adds either a warning or error, based on the current handling of
  // -Werror.
  DiagnosticsEngine::Level getErrorLevel() {
    return diagnostic().getWarningsAsErrors() ?
        DiagnosticsEngine::Error : DiagnosticsEngine::Warning;
  }

  // Returns true if |base| specifies one of the Chromium reference counted
  // classes (base::RefCounted / base::RefCountedThreadSafe).
  static bool IsRefCountedCallback(const CXXBaseSpecifier* base,
                                   CXXBasePath& path,
                                   void* user_data) {
    FindBadConstructsConsumer* self =
        static_cast<FindBadConstructsConsumer*>(user_data);

    const TemplateSpecializationType* base_type =
        dyn_cast<TemplateSpecializationType>(
            UnwrapType(base->getType().getTypePtr()));
    if (!base_type) {
      // Base-most definition is not a template, so this cannot derive from
      // base::RefCounted. However, it may still be possible to use with a
      // scoped_refptr<> and support ref-counting, so this is not a perfect
      // guarantee of safety.
      return false;
    }

    TemplateName name = base_type->getTemplateName();
    if (TemplateDecl* decl = name.getAsTemplateDecl()) {
      std::string base_name = decl->getNameAsString();

      // Check for both base::RefCounted and base::RefCountedThreadSafe.
      if (base_name.compare(0, 10, "RefCounted") == 0 &&
          self->GetNamespace(decl) == "base") {
        return true;
      }
    }

    return false;
  }

  // Returns true if |base| specifies a class that has a public destructor,
  // either explicitly or implicitly.
  static bool HasPublicDtorCallback(const CXXBaseSpecifier* base,
                                    CXXBasePath& path,
                                    void* user_data) {
    // Only examine paths that have public inheritance, as they are the
    // only ones which will result in the destructor potentially being
    // exposed. This check is largely redundant, as Chromium code should be
    // exclusively using public inheritance.
    if (path.Access != AS_public)
      return false;

    CXXRecordDecl* record = dyn_cast<CXXRecordDecl>(
        base->getType()->getAs<RecordType>()->getDecl());
    SourceLocation unused;
    return None != CheckRecordForRefcountIssue(record, unused);
  }

  // Outputs a C++ inheritance chain as a diagnostic aid.
  void PrintInheritanceChain(const CXXBasePath& path) {
    for (CXXBasePath::const_iterator it = path.begin(); it != path.end();
         ++it) {
      diagnostic().Report(it->Base->getLocStart(), diag_note_inheritance_)
          << it->Class << it->Base->getType();
    }
  }

  unsigned DiagnosticForIssue(RefcountIssue issue) {
    switch (issue) {
      case ImplicitDestructor:
        return diag_no_explicit_dtor_;
      case PublicDestructor:
        return diag_public_dtor_;
      case None:
        assert(false && "Do not call DiagnosticForIssue with issue None");
        return 0;
    }
  }

  // Check |record| to determine if it has any problematic refcounting
  // issues and, if so, print them as warnings/errors based on the current
  // value of getErrorLevel().
  //
  // If |record| is a C++ class, and if it inherits from one of the Chromium
  // ref-counting classes (base::RefCounted / base::RefCountedThreadSafe),
  // ensure that there are no public destructors in the class hierarchy. This
  // is to guard against accidentally stack-allocating a RefCounted class or
  // sticking it in a non-ref-counted container (like scoped_ptr<>).
  void CheckRefCountedDtors(SourceLocation record_location,
                            CXXRecordDecl* record) {
    // Skip anonymous structs.
    if (record->getIdentifier() == NULL)
      return;

    // Determine if the current type is even ref-counted.
    CXXBasePaths refcounted_path;
    if (!record->lookupInBases(
            &FindBadConstructsConsumer::IsRefCountedCallback, this,
            refcounted_path)) {
      return;  // Class does not derive from a ref-counted base class.
    }

    // Easy check: Check to see if the current type is problematic.
    SourceLocation loc;
    RefcountIssue issue = CheckRecordForRefcountIssue(record, loc);
    if (issue != None) {
      diagnostic().Report(loc, DiagnosticForIssue(issue));
      PrintInheritanceChain(refcounted_path.front());
      return;
    }
    if (CXXDestructorDecl* dtor =
        refcounted_path.begin()->back().Class->getDestructor()) {
      if (dtor->getAccess() == AS_protected &&
          !dtor->isVirtual()) {
        loc = dtor->getInnerLocStart();
        diagnostic().Report(loc, diag_protected_non_virtual_dtor_);
        return;
      }
    }

    // Long check: Check all possible base classes for problematic
    // destructors. This checks for situations involving multiple
    // inheritance, where the ref-counted class may be implementing an
    // interface that has a public or implicit destructor.
    //
    // struct SomeInterface {
    //   virtual void DoFoo();
    // };
    //
    // struct RefCountedInterface
    //    : public base::RefCounted<RefCountedInterface>,
    //      public SomeInterface {
    //  private:
    //   friend class base::Refcounted<RefCountedInterface>;
    //   virtual ~RefCountedInterface() {}
    // };
    //
    // While RefCountedInterface is "safe", in that its destructor is
    // private, it's possible to do the following "unsafe" code:
    //   scoped_refptr<RefCountedInterface> some_class(
    //       new RefCountedInterface);
    //   // Calls SomeInterface::~SomeInterface(), which is unsafe.
    //   delete static_cast<SomeInterface*>(some_class.get());
    if (!check_base_classes_)
      return;

    // Find all public destructors. This will record the class hierarchy
    // that leads to the public destructor in |dtor_paths|.
    CXXBasePaths dtor_paths;
    if (!record->lookupInBases(
            &FindBadConstructsConsumer::HasPublicDtorCallback, this,
            dtor_paths)) {
      return;
    }

    for (CXXBasePaths::const_paths_iterator it = dtor_paths.begin();
         it != dtor_paths.end(); ++it) {
      // The record with the problem will always be the last record
      // in the path, since it is the record that stopped the search.
      const CXXRecordDecl* problem_record = dyn_cast<CXXRecordDecl>(
          it->back().Base->getType()->getAs<RecordType>()->getDecl());

      issue = CheckRecordForRefcountIssue(problem_record, loc);

      if (issue == ImplicitDestructor) {
        diagnostic().Report(record_location, diag_no_explicit_dtor_);
        PrintInheritanceChain(refcounted_path.front());
        diagnostic().Report(loc, diag_note_implicit_dtor_) << problem_record;
        PrintInheritanceChain(*it);
      } else if (issue == PublicDestructor) {
        diagnostic().Report(record_location, diag_public_dtor_);
        PrintInheritanceChain(refcounted_path.front());
        diagnostic().Report(loc, diag_note_public_dtor_);
        PrintInheritanceChain(*it);
      }
    }
  }
};

class FindBadConstructsAction : public PluginASTAction {
 public:
  FindBadConstructsAction()
      : check_base_classes_(false),
        check_virtuals_in_implementations_(true),
        check_url_directory_(false) {
  }

 protected:
  // Overridden from PluginASTAction:
  virtual ASTConsumer* CreateASTConsumer(CompilerInstance& instance,
                                         llvm::StringRef ref) {
    return new FindBadConstructsConsumer(
        instance, check_base_classes_, check_virtuals_in_implementations_,
        check_url_directory_);
  }

  virtual bool ParseArgs(const CompilerInstance& instance,
                         const std::vector<std::string>& args) {
    bool parsed = true;

    for (size_t i = 0; i < args.size() && parsed; ++i) {
      if (args[i] == "skip-virtuals-in-implementations") {
        // TODO(rsleevi): Remove this once http://crbug.com/115047 is fixed.
        check_virtuals_in_implementations_ = false;
      } else if (args[i] == "check-base-classes") {
        // TODO(rsleevi): Remove this once http://crbug.com/123295 is fixed.
        check_base_classes_ = true;
      } else if (args[i] == "check-url-directory") {
        // TODO(tfarina): Remove this once http://crbug.com/229660 is fixed.
        check_url_directory_ = true;
      } else {
        parsed = false;
        llvm::errs() << "Unknown clang plugin argument: " << args[i] << "\n";
      }
    }

    return parsed;
  }

 private:
  bool check_base_classes_;
  bool check_virtuals_in_implementations_;
  bool check_url_directory_;
};

}  // namespace

static FrontendPluginRegistry::Add<FindBadConstructsAction>
X("find-bad-constructs", "Finds bad C++ constructs");
