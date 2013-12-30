#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "mpd/base/mpd_builder.h"
#include "mpd/base/xml/scoped_xml_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libxml/src/include/libxml/parser.h"

namespace dash_packager {
namespace {

// This was validated at a validator site below as well.
// http://www-itec.uni-klu.ac.at/dash/?page_id=605#
const char kValidMpd[] = "<?xml version='1.0' encoding='UTF-8'?>\n\
<MPD\n\
 xmlns='urn:mpeg:DASH:schema:MPD:2011'\n\
 xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'\n\
 xmlns:xlink='http://www.w3.org/1999/xlink'\n\
 xsi:schemaLocation='urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd'\n\
 minBufferTime='PT2S'\n\
 type='static'\n\
 profiles='urn:mpeg:dash:profile:isoff-on-demand:2011'\n\
 mediaPresentationDuration='PT101S'>\n\
  <BaseURL>http://cdn1.example.com/</BaseURL>\n\
  <Period>\n\
    <AdaptationSet id='0'>\n\
      <Representation id='0' bandwidth='20000' mimeType='video/mp4' codecs='avc1'>\n\
        <BaseURL>\n\
         something.mp4\n\
        </BaseURL>\n\
      </Representation>\n\
      <Representation id='1' bandwidth='20000' mimeType='video/mp4' codecs='avc1'>\n\
        <BaseURL>\n\
         somethingelse.mp4\n\
        </BaseURL>\n\
      </Representation>\n\
    </AdaptationSet>\n\
  </Period>\n\
</MPD>\n\
";
const size_t kValidMpdSize = arraysize(kValidMpd) - 1;  // Exclude '\0'.

base::FilePath GetSchemaPath() {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("mpd"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("schema"))
      .Append(FILE_PATH_LITERAL("DASH-MPD.xsd"));
  return file_path;
}

std::string GetPathContent(const base::FilePath& file_path) {
  std::string content;
  bool file_read_to_string = file_util::ReadFileToString(file_path, &content);
  DCHECK(file_read_to_string);
  return content;
}

bool ValidateMpdSchema(const std::string& mpd) {
  xml::ScopedXmlPtr<xmlDoc>::type doc(xmlParseMemory(mpd.data(), mpd.size()));
  if (!doc) {
    LOG(ERROR) << "Failed to parse mpd into an xml doc.";
    return false;
  }

  base::FilePath schema_path = GetSchemaPath();
  std::string schema_str = GetPathContent(schema_path);

  // First, I need to load the schema as a xmlDoc so that I can pass the path of
  // the DASH-MPD.xsd. Then it can resolve the relative path included from the
  // XSD when creating xmlSchemaParserCtxt.
  xml::ScopedXmlPtr<xmlDoc>::type schema_as_doc(
      xmlReadMemory(schema_str.data(),
                    schema_str.size(),
                    schema_path.value().c_str(),
                    NULL,
                    0));
  DCHECK(schema_as_doc);
  xml::ScopedXmlPtr<xmlSchemaParserCtxt>::type schema_parser_ctxt(
      xmlSchemaNewDocParserCtxt(schema_as_doc.get()));
  DCHECK(schema_parser_ctxt);

  xml::ScopedXmlPtr<xmlSchema>::type schema(
      xmlSchemaParse(schema_parser_ctxt.get()));
  DCHECK(schema);

  xml::ScopedXmlPtr<xmlSchemaValidCtxt>::type valid_ctxt(
      xmlSchemaNewValidCtxt(schema.get()));
  DCHECK(valid_ctxt);
  int validation_result =
      xmlSchemaValidateDoc(valid_ctxt.get(), doc.get());
  DLOG(INFO) << "XSD validation result: " << validation_result;
  return validation_result == 0;
}

}  // namespace

// Check if the schema validation works. If not, then most of the tests would
// probably fail.
TEST(MpdSchemaMetaTest, CheckSchemaValidatorWorks) {
  ASSERT_TRUE(ValidateMpdSchema(std::string(kValidMpd, kValidMpdSize)));
}

// TODO(rkuroiwa): MPD builder does not build a valid MPD yet. Enable these when
// its done. Make sure to compare against a known MPD to validate.
// A normal use case where there are 2 adaptation sets and 2 representations.
TEST(MpdBuilder, DISABLED_VOD_Normal) {
  MpdBuilder mpd(MpdBuilder::kStatic);
  AdaptationSet* adaptation_set = mpd.AddAdaptationSet();
  ASSERT_TRUE(adaptation_set);

  AdaptationSet* adaptation_set2 = mpd.AddAdaptationSet();
  ASSERT_TRUE(adaptation_set2);

  MediaInfo media_info;
  media_info.set_bandwidth(100);
  ASSERT_TRUE(adaptation_set->AddRepresentation(media_info));
  ASSERT_TRUE(adaptation_set2->AddRepresentation(media_info));

  std::string mpd_doc;
  ASSERT_TRUE(mpd.ToString(&mpd_doc));
  ASSERT_TRUE(ValidateMpdSchema(mpd_doc));
}

// Different media duration should not error.
TEST(MpdBuilder, DISABLED_VOD_DifferentMediaDuration) {
  MpdBuilder mpd(MpdBuilder::kStatic);
  AdaptationSet* adaptation_set = mpd.AddAdaptationSet();
  ASSERT_TRUE(adaptation_set);

  MediaInfo media_info;
  media_info.set_bandwidth(20000);
  media_info.set_media_duration_seconds(100);
  ASSERT_TRUE(adaptation_set->AddRepresentation(media_info));

  media_info.set_media_duration_seconds(101);
  ASSERT_TRUE(adaptation_set->AddRepresentation(media_info));

  std::string mpd_doc;
  ASSERT_TRUE(mpd.ToString(&mpd_doc));
  ASSERT_TRUE(ValidateMpdSchema(mpd_doc));
}

}  // namespace dash_packager
