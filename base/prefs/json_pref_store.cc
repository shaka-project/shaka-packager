// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/json_pref_store.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"

namespace {

// Some extensions we'll tack on to copies of the Preferences files.
const base::FilePath::CharType* kBadExtension = FILE_PATH_LITERAL("bad");

// Differentiates file loading between origin thread and passed
// (aka file) thread.
class FileThreadDeserializer
    : public base::RefCountedThreadSafe<FileThreadDeserializer> {
 public:
  FileThreadDeserializer(JsonPrefStore* delegate,
                         base::SequencedTaskRunner* sequenced_task_runner)
      : no_dir_(false),
        error_(PersistentPrefStore::PREF_READ_ERROR_NONE),
        delegate_(delegate),
        sequenced_task_runner_(sequenced_task_runner),
        origin_loop_proxy_(base::MessageLoopProxy::current()) {
  }

  void Start(const base::FilePath& path) {
    DCHECK(origin_loop_proxy_->BelongsToCurrentThread());
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&FileThreadDeserializer::ReadFileAndReport,
                   this, path));
  }

  // Deserializes JSON on the sequenced task runner.
  void ReadFileAndReport(const base::FilePath& path) {
    DCHECK(sequenced_task_runner_->RunsTasksOnCurrentThread());

    value_.reset(DoReading(path, &error_, &no_dir_));

    origin_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&FileThreadDeserializer::ReportOnOriginThread, this));
  }

  // Reports deserialization result on the origin thread.
  void ReportOnOriginThread() {
    DCHECK(origin_loop_proxy_->BelongsToCurrentThread());
    delegate_->OnFileRead(value_.release(), error_, no_dir_);
  }

  static base::Value* DoReading(const base::FilePath& path,
                                PersistentPrefStore::PrefReadError* error,
                                bool* no_dir) {
    int error_code;
    std::string error_msg;
    JSONFileValueSerializer serializer(path);
    base::Value* value = serializer.Deserialize(&error_code, &error_msg);
    HandleErrors(value, path, error_code, error_msg, error);
    *no_dir = !base::PathExists(path.DirName());
    return value;
  }

  static void HandleErrors(const base::Value* value,
                           const base::FilePath& path,
                           int error_code,
                           const std::string& error_msg,
                           PersistentPrefStore::PrefReadError* error);

 private:
  friend class base::RefCountedThreadSafe<FileThreadDeserializer>;
  ~FileThreadDeserializer() {}

  bool no_dir_;
  PersistentPrefStore::PrefReadError error_;
  scoped_ptr<base::Value> value_;
  const scoped_refptr<JsonPrefStore> delegate_;
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  const scoped_refptr<base::MessageLoopProxy> origin_loop_proxy_;
};

// static
void FileThreadDeserializer::HandleErrors(
    const base::Value* value,
    const base::FilePath& path,
    int error_code,
    const std::string& error_msg,
    PersistentPrefStore::PrefReadError* error) {
  *error = PersistentPrefStore::PREF_READ_ERROR_NONE;
  if (!value) {
    DVLOG(1) << "Error while loading JSON file: " << error_msg
             << ", file: " << path.value();
    switch (error_code) {
      case JSONFileValueSerializer::JSON_ACCESS_DENIED:
        *error = PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED;
        break;
      case JSONFileValueSerializer::JSON_CANNOT_READ_FILE:
        *error = PersistentPrefStore::PREF_READ_ERROR_FILE_OTHER;
        break;
      case JSONFileValueSerializer::JSON_FILE_LOCKED:
        *error = PersistentPrefStore::PREF_READ_ERROR_FILE_LOCKED;
        break;
      case JSONFileValueSerializer::JSON_NO_SUCH_FILE:
        *error = PersistentPrefStore::PREF_READ_ERROR_NO_FILE;
        break;
      default:
        *error = PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE;
        // JSON errors indicate file corruption of some sort.
        // Since the file is corrupt, move it to the side and continue with
        // empty preferences.  This will result in them losing their settings.
        // We keep the old file for possible support and debugging assistance
        // as well as to detect if they're seeing these errors repeatedly.
        // TODO(erikkay) Instead, use the last known good file.
        base::FilePath bad = path.ReplaceExtension(kBadExtension);

        // If they've ever had a parse error before, put them in another bucket.
        // TODO(erikkay) if we keep this error checking for very long, we may
        // want to differentiate between recent and long ago errors.
        if (base::PathExists(bad))
          *error = PersistentPrefStore::PREF_READ_ERROR_JSON_REPEAT;
        base::Move(path, bad);
        break;
    }
  } else if (!value->IsType(base::Value::TYPE_DICTIONARY)) {
    *error = PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE;
  }
}

}  // namespace

scoped_refptr<base::SequencedTaskRunner> JsonPrefStore::GetTaskRunnerForFile(
    const base::FilePath& filename,
    base::SequencedWorkerPool* worker_pool) {
  std::string token("json_pref_store-");
  token.append(filename.AsUTF8Unsafe());
  return worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
      worker_pool->GetNamedSequenceToken(token),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

JsonPrefStore::JsonPrefStore(const base::FilePath& filename,
                             base::SequencedTaskRunner* sequenced_task_runner)
    : path_(filename),
      sequenced_task_runner_(sequenced_task_runner),
      prefs_(new base::DictionaryValue()),
      read_only_(false),
      writer_(filename, sequenced_task_runner),
      initialized_(false),
      read_error_(PREF_READ_ERROR_OTHER) {}

bool JsonPrefStore::GetValue(const std::string& key,
                             const base::Value** result) const {
  base::Value* tmp = NULL;
  if (!prefs_->Get(key, &tmp))
    return false;

  if (result)
    *result = tmp;
  return true;
}

void JsonPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void JsonPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

size_t JsonPrefStore::NumberOfObservers() const {
  return observers_.size();
}

bool JsonPrefStore::IsInitializationComplete() const {
  return initialized_;
}

bool JsonPrefStore::GetMutableValue(const std::string& key,
                                    base::Value** result) {
  return prefs_->Get(key, result);
}

void JsonPrefStore::SetValue(const std::string& key, base::Value* value) {
  DCHECK(value);
  scoped_ptr<base::Value> new_value(value);
  base::Value* old_value = NULL;
  prefs_->Get(key, &old_value);
  if (!old_value || !value->Equals(old_value)) {
    prefs_->Set(key, new_value.release());
    ReportValueChanged(key);
  }
}

void JsonPrefStore::SetValueSilently(const std::string& key,
                                     base::Value* value) {
  DCHECK(value);
  scoped_ptr<base::Value> new_value(value);
  base::Value* old_value = NULL;
  prefs_->Get(key, &old_value);
  if (!old_value || !value->Equals(old_value)) {
    prefs_->Set(key, new_value.release());
    if (!read_only_)
      writer_.ScheduleWrite(this);
  }
}

void JsonPrefStore::RemoveValue(const std::string& key) {
  if (prefs_->Remove(key, NULL))
    ReportValueChanged(key);
}

void JsonPrefStore::MarkNeedsEmptyValue(const std::string& key) {
  keys_need_empty_value_.insert(key);
}

bool JsonPrefStore::ReadOnly() const {
  return read_only_;
}

PersistentPrefStore::PrefReadError JsonPrefStore::GetReadError() const {
  return read_error_;
}

PersistentPrefStore::PrefReadError JsonPrefStore::ReadPrefs() {
  if (path_.empty()) {
    OnFileRead(NULL, PREF_READ_ERROR_FILE_NOT_SPECIFIED, false);
    return PREF_READ_ERROR_FILE_NOT_SPECIFIED;
  }

  PrefReadError error;
  bool no_dir;
  base::Value* value =
      FileThreadDeserializer::DoReading(path_, &error, &no_dir);
  OnFileRead(value, error, no_dir);
  return error;
}

void JsonPrefStore::ReadPrefsAsync(ReadErrorDelegate *error_delegate) {
  initialized_ = false;
  error_delegate_.reset(error_delegate);
  if (path_.empty()) {
    OnFileRead(NULL, PREF_READ_ERROR_FILE_NOT_SPECIFIED, false);
    return;
  }

  // Start async reading of the preferences file. It will delete itself
  // in the end.
  scoped_refptr<FileThreadDeserializer> deserializer(
      new FileThreadDeserializer(this, sequenced_task_runner_.get()));
  deserializer->Start(path_);
}

void JsonPrefStore::CommitPendingWrite() {
  if (writer_.HasPendingWrite() && !read_only_)
    writer_.DoScheduledWrite();
}

void JsonPrefStore::ReportValueChanged(const std::string& key) {
  FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));
  if (!read_only_)
    writer_.ScheduleWrite(this);
}

void JsonPrefStore::OnFileRead(base::Value* value_owned,
                               PersistentPrefStore::PrefReadError error,
                               bool no_dir) {
  scoped_ptr<base::Value> value(value_owned);
  read_error_ = error;

  if (no_dir) {
    FOR_EACH_OBSERVER(PrefStore::Observer,
                      observers_,
                      OnInitializationCompleted(false));
    return;
  }

  initialized_ = true;

  switch (error) {
    case PREF_READ_ERROR_ACCESS_DENIED:
    case PREF_READ_ERROR_FILE_OTHER:
    case PREF_READ_ERROR_FILE_LOCKED:
    case PREF_READ_ERROR_JSON_TYPE:
    case PREF_READ_ERROR_FILE_NOT_SPECIFIED:
      read_only_ = true;
      break;
    case PREF_READ_ERROR_NONE:
      DCHECK(value.get());
      prefs_.reset(static_cast<base::DictionaryValue*>(value.release()));
      break;
    case PREF_READ_ERROR_NO_FILE:
      // If the file just doesn't exist, maybe this is first run.  In any case
      // there's no harm in writing out default prefs in this case.
      break;
    case PREF_READ_ERROR_JSON_PARSE:
    case PREF_READ_ERROR_JSON_REPEAT:
      break;
    default:
      NOTREACHED() << "Unknown error: " << error;
  }

  if (error_delegate_.get() && error != PREF_READ_ERROR_NONE)
    error_delegate_->OnError(error);

  FOR_EACH_OBSERVER(PrefStore::Observer,
                    observers_,
                    OnInitializationCompleted(true));
}

JsonPrefStore::~JsonPrefStore() {
  CommitPendingWrite();
}

bool JsonPrefStore::SerializeData(std::string* output) {
  // TODO(tc): Do we want to prune webkit preferences that match the default
  // value?
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  scoped_ptr<base::DictionaryValue> copy(
      prefs_->DeepCopyWithoutEmptyChildren());

  // Iterates |keys_need_empty_value_| and if the key exists in |prefs_|,
  // ensure its empty ListValue or DictonaryValue is preserved.
  for (std::set<std::string>::const_iterator
       it = keys_need_empty_value_.begin();
       it != keys_need_empty_value_.end();
       ++it) {
    const std::string& key = *it;

    base::Value* value = NULL;
    if (!prefs_->Get(key, &value))
      continue;

    if (value->IsType(base::Value::TYPE_LIST)) {
      const base::ListValue* list = NULL;
      if (value->GetAsList(&list) && list->empty())
        copy->Set(key, new base::ListValue);
    } else if (value->IsType(base::Value::TYPE_DICTIONARY)) {
      const base::DictionaryValue* dict = NULL;
      if (value->GetAsDictionary(&dict) && dict->empty())
        copy->Set(key, new base::DictionaryValue);
    }
  }

  return serializer.Serialize(*(copy.get()));
}
