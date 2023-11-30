// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/language_utils.h>

#include <iterator>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace {

// A map from 3-letter language codes (ISO 639-2) to 2-letter language codes
// (ISO 639-1) for all languages which have both in the registry.
typedef struct {
  const char iso_639_2[4];  // 3 letters + nul
  const char iso_639_1[3];  // 2 letters + nul
} LanguageMapPairType;
const LanguageMapPairType kLanguageMap[] = {
  { "aar", "aa" }, { "abk", "ab" }, { "afr", "af" }, { "aka", "ak" },
  { "alb", "sq" }, { "amh", "am" }, { "ara", "ar" }, { "arg", "an" },
  { "arm", "hy" }, { "asm", "as" }, { "ava", "av" }, { "ave", "ae" },
  { "aym", "ay" }, { "aze", "az" }, { "bak", "ba" }, { "bam", "bm" },
  { "baq", "eu" }, { "bel", "be" }, { "ben", "bn" }, { "bih", "bh" },
  { "bis", "bi" }, { "bod", "bo" }, { "bos", "bs" }, { "bre", "br" },
  { "bul", "bg" }, { "bur", "my" }, { "cat", "ca" }, { "ces", "cs" },
  { "cha", "ch" }, { "che", "ce" }, { "chi", "zh" }, { "chu", "cu" },
  { "chv", "cv" }, { "cor", "kw" }, { "cos", "co" }, { "cre", "cr" },
  { "cym", "cy" }, { "cze", "cs" }, { "dan", "da" }, { "deu", "de" },
  { "div", "dv" }, { "dut", "nl" }, { "dzo", "dz" }, { "ell", "el" },
  { "eng", "en" }, { "epo", "eo" }, { "est", "et" }, { "eus", "eu" },
  { "ewe", "ee" }, { "fao", "fo" }, { "fas", "fa" }, { "fij", "fj" },
  { "fin", "fi" }, { "fra", "fr" }, { "fre", "fr" }, { "fry", "fy" },
  { "ful", "ff" }, { "geo", "ka" }, { "ger", "de" }, { "gla", "gd" },
  { "gle", "ga" }, { "glg", "gl" }, { "glv", "gv" }, { "gre", "el" },
  { "grn", "gn" }, { "guj", "gu" }, { "hat", "ht" }, { "hau", "ha" },
  { "heb", "he" }, { "her", "hz" }, { "hin", "hi" }, { "hmo", "ho" },
  { "hrv", "hr" }, { "hun", "hu" }, { "hye", "hy" }, { "ibo", "ig" },
  { "ice", "is" }, { "ido", "io" }, { "iii", "ii" }, { "iku", "iu" },
  { "ile", "ie" }, { "ina", "ia" }, { "ind", "id" }, { "ipk", "ik" },
  { "isl", "is" }, { "ita", "it" }, { "jav", "jv" }, { "jpn", "ja" },
  { "kal", "kl" }, { "kan", "kn" }, { "kas", "ks" }, { "kat", "ka" },
  { "kau", "kr" }, { "kaz", "kk" }, { "khm", "km" }, { "kik", "ki" },
  { "kin", "rw" }, { "kir", "ky" }, { "kom", "kv" }, { "kon", "kg" },
  { "kor", "ko" }, { "kua", "kj" }, { "kur", "ku" }, { "lao", "lo" },
  { "lat", "la" }, { "lav", "lv" }, { "lim", "li" }, { "lin", "ln" },
  { "lit", "lt" }, { "ltz", "lb" }, { "lub", "lu" }, { "lug", "lg" },
  { "mac", "mk" }, { "mah", "mh" }, { "mal", "ml" }, { "mao", "mi" },
  { "mar", "mr" }, { "may", "ms" }, { "mkd", "mk" }, { "mlg", "mg" },
  { "mlt", "mt" }, { "mon", "mn" }, { "mri", "mi" }, { "msa", "ms" },
  { "mya", "my" }, { "nau", "na" }, { "nav", "nv" }, { "nbl", "nr" },
  { "nde", "nd" }, { "ndo", "ng" }, { "nep", "ne" }, { "nld", "nl" },
  { "nno", "nn" }, { "nob", "nb" }, { "nor", "no" }, { "nya", "ny" },
  { "oci", "oc" }, { "oji", "oj" }, { "ori", "or" }, { "orm", "om" },
  { "oss", "os" }, { "pan", "pa" }, { "per", "fa" }, { "pli", "pi" },
  { "pol", "pl" }, { "por", "pt" }, { "pus", "ps" }, { "que", "qu" },
  { "roh", "rm" }, { "ron", "ro" }, { "rum", "ro" }, { "run", "rn" },
  { "rus", "ru" }, { "sag", "sg" }, { "san", "sa" }, { "sin", "si" },
  { "slk", "sk" }, { "slo", "sk" }, { "slv", "sl" }, { "sme", "se" },
  { "smo", "sm" }, { "sna", "sn" }, { "snd", "sd" }, { "som", "so" },
  { "sot", "st" }, { "spa", "es" }, { "sqi", "sq" }, { "srd", "sc" },
  { "srp", "sr" }, { "ssw", "ss" }, { "sun", "su" }, { "swa", "sw" },
  { "swe", "sv" }, { "tah", "ty" }, { "tam", "ta" }, { "tat", "tt" },
  { "tel", "te" }, { "tgk", "tg" }, { "tgl", "tl" }, { "tha", "th" },
  { "tib", "bo" }, { "tir", "ti" }, { "ton", "to" }, { "tsn", "tn" },
  { "tso", "ts" }, { "tuk", "tk" }, { "tur", "tr" }, { "twi", "tw" },
  { "uig", "ug" }, { "ukr", "uk" }, { "urd", "ur" }, { "uzb", "uz" },
  { "ven", "ve" }, { "vie", "vi" }, { "vol", "vo" }, { "wel", "cy" },
  { "wln", "wa" }, { "wol", "wo" }, { "xho", "xh" }, { "yid", "yi" },
  { "yor", "yo" }, { "zha", "za" }, { "zho", "zh" }, { "zul", "zu" },
};

void SplitLanguageTag(const std::string& tag,
                      std::string* main_language, std::string* subtag) {
  // Split the main language from its subtag (if any).
  *main_language = tag;
  subtag->clear();
  size_t dash = main_language->find('-');
  if (dash != std::string::npos) {
    *subtag = main_language->substr(dash);
    main_language->erase(dash);
  }
}

}  // namespace

namespace shaka {

std::string LanguageToShortestForm(const std::string& language) {
  // Do not try to mangle blank strings.
  if (language.size() == 0) {
    return language;
  }

  std::string main_language;
  std::string subtag;
  SplitLanguageTag(language, &main_language, &subtag);

  if (main_language.size() == 2) {
    // Presumably already a valid ISO-639-1 code, and therefore conforms to
    // BCP-47's requirement to use the shortest possible code.
    return main_language + subtag;
  }

  for (size_t i = 0; i < std::size(kLanguageMap); ++i) {
    if (main_language == kLanguageMap[i].iso_639_2) {
      return kLanguageMap[i].iso_639_1 + subtag;
    }
  }

  // This could happen legitimately for languages which have no 2-letter code,
  // but that would imply that the input language code is a 3-letter code.
  DCHECK_EQ(3u, main_language.size()) << main_language;
  return main_language + subtag;
}

std::string LanguageToISO_639_2(const std::string& language) {
  std::string main_language;
  std::string subtag;
  SplitLanguageTag(language, &main_language, &subtag);

  if (main_language.size() == 3) {
    // Presumably already a valid ISO-639-2 code.
    return main_language + subtag;
  }

  for (size_t i = 0; i < std::size(kLanguageMap); ++i) {
    if (main_language == kLanguageMap[i].iso_639_1) {
      return kLanguageMap[i].iso_639_2 + subtag;
    }
  }

  LOG(WARNING) << "No equivalent 3-letter language code for " << main_language;
  // This is probably a mistake on the part of the user and should be treated
  // as invalid input.
  return "und";
}

}  // namespace shaka
