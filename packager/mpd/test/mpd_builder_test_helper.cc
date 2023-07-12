// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/test/mpd_builder_test_helper.h"

#include <glog/logging.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <filesystem>

#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/xml/scoped_xml_ptr.h"
#include "packager/mpd/test/xml_compare.h"

namespace shaka {

std::filesystem::path GetTestDataFilePath(const std::string& name) {
  std::filesystem::path header_path(__FILE__);
  return header_path.parent_path() / "data" / name;
}

std::filesystem::path GetSchemaPath() {
  std::filesystem::path header_path(__FILE__);
  return header_path.parent_path() / "schema" / "DASH-MPD.xsd";
}

std::string GetPathContent(std::filesystem::path& file_path) {
  std::string content;

  FILE* f = fopen(file_path.string().c_str(), "rb");
  if (!f) {
    LOG(FATAL) << "Failed to read test data from " << file_path;
    return std::string{};
  }

  content.resize(std::filesystem::file_size(file_path));
  size_t size = fread(content.data(), 1, content.size(), f);
  content.resize(size);
  fclose(f);

  return content;
}

MediaInfo ConvertToMediaInfo(const std::string& media_info_string) {
  MediaInfo media_info;
  CHECK(::google::protobuf::TextFormat::ParseFromString(media_info_string,
                                                        &media_info));
  return media_info;
}

MediaInfo GetTestMediaInfo(const std::string& media_info_file_name) {
  std::filesystem::path test_path = GetTestDataFilePath(media_info_file_name);
  return ConvertToMediaInfo(GetPathContent(test_path));
}

bool ValidateMpdSchema(const std::string& mpd) {
  xml::scoped_xml_ptr<xmlDoc> doc(
      xmlParseMemory(mpd.data(), mpd.size()));
  if (!doc) {
    LOG(ERROR) << "Failed to parse mpd into an xml doc.";
    return false;
  }

  std::filesystem::path schema_path = GetSchemaPath();
  std::string schema_str = GetPathContent(schema_path);

  // First, I need to load the schema as a xmlDoc so that I can pass the path of
  // the DASH-MPD.xsd. Then it can resolve the relative path included from the
  // XSD when creating xmlSchemaParserCtxt.
  xml::scoped_xml_ptr<xmlDoc> schema_as_doc(
      xmlReadMemory(schema_str.data(), schema_str.size(),
                    schema_path.string().c_str(), NULL, 0));
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
  std::filesystem::path expected_output_file_path =
      GetTestDataFilePath(expected_output_file);
  std::string expected_mpd = GetPathContent(expected_output_file_path);

  ASSERT_TRUE(!expected_mpd.empty())
      << "Failed to read: " << expected_output_file;

  // Adding extra << here to get a formatted output.
  ASSERT_TRUE(XmlEqual(expected_mpd, mpd_string))
      << "Expected:" << std::endl << expected_mpd << std::endl
      << "Actual:" << std::endl << mpd_string;
}

}  // namespace shaka
