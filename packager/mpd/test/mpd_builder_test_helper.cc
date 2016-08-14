// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/test/mpd_builder_test_helper.h"

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "packager/base/files/file_util.h"
#include "packager/base/path_service.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/xml/scoped_xml_ptr.h"
#include "packager/mpd/test/xml_compare.h"

namespace shaka {

base::FilePath GetTestDataFilePath(const std::string& file_name) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("packager"))
      .Append(FILE_PATH_LITERAL("mpd"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .AppendASCII(file_name);
  return file_path;
}

base::FilePath GetSchemaPath() {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("packager"))
      .Append(FILE_PATH_LITERAL("mpd"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("schema"))
      .Append(FILE_PATH_LITERAL("DASH-MPD.xsd"));
  return file_path;
}

std::string GetPathContent(const base::FilePath& file_path) {
  std::string content;
  bool file_read_to_string = base::ReadFileToString(file_path, &content);
  DCHECK(file_read_to_string) << file_path.value();
  return content;
}

MediaInfo ConvertToMediaInfo(const std::string& media_info_string) {
  MediaInfo media_info;
  CHECK(::google::protobuf::TextFormat::ParseFromString(media_info_string,
                                                        &media_info));
  return media_info;
}

MediaInfo GetTestMediaInfo(const std::string& media_info_file_name) {
  return ConvertToMediaInfo(
      GetPathContent(GetTestDataFilePath(media_info_file_name)));
}

bool ValidateMpdSchema(const std::string& mpd) {
  xml::scoped_xml_ptr<xmlDoc> doc(
      xmlParseMemory(mpd.data(), mpd.size()));
  if (!doc) {
    LOG(ERROR) << "Failed to parse mpd into an xml doc.";
    return false;
  }

  base::FilePath schema_path = GetSchemaPath();
  std::string schema_str = GetPathContent(schema_path);

  // First, I need to load the schema as a xmlDoc so that I can pass the path of
  // the DASH-MPD.xsd. Then it can resolve the relative path included from the
  // XSD when creating xmlSchemaParserCtxt.
  xml::scoped_xml_ptr<xmlDoc> schema_as_doc(
      xmlReadMemory(schema_str.data(),
                    schema_str.size(),
                    schema_path.AsUTF8Unsafe().c_str(),
                    NULL,
                    0));
  DCHECK(schema_as_doc);
  xml::scoped_xml_ptr<xmlSchemaParserCtxt>
      schema_parser_ctxt(xmlSchemaNewDocParserCtxt(schema_as_doc.get()));
  DCHECK(schema_parser_ctxt);

  xml::scoped_xml_ptr<xmlSchema> schema(
      xmlSchemaParse(schema_parser_ctxt.get()));
  DCHECK(schema);

  xml::scoped_xml_ptr<xmlSchemaValidCtxt> valid_ctxt(
      xmlSchemaNewValidCtxt(schema.get()));
  DCHECK(valid_ctxt);
  int validation_result =
      xmlSchemaValidateDoc(valid_ctxt.get(), doc.get());
  DLOG(INFO) << "XSD validation result: " << validation_result;
  return validation_result == 0;
}

void ExpectMpdToEqualExpectedOutputFile(
    const std::string& mpd_string,
    const std::string& expected_output_file) {
  std::string expected_mpd;
  ASSERT_TRUE(base::ReadFileToString(GetTestDataFilePath(expected_output_file),
                                     &expected_mpd))
      << "Failed to read: " << expected_output_file;

  // Adding extra << here to get a formatted output.
  ASSERT_TRUE(XmlEqual(expected_mpd, mpd_string))
      << "Expected:" << std::endl << expected_mpd << std::endl
      << "Actual:" << std::endl << mpd_string;
}

}  // namespace shaka
