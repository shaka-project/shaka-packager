Media packaging SDK is intended for C++ programmers writing DASH packager application with common encryption support.

This document provides the information needed to create a DASH packager that is able to remux and encrypt a video into fragmented ISO BMFF format with CENC support. The DASH packaging API is also designed in such a way for easy extension to more source and destination formats.

# Setting up for development #

1. Packager source is managed by Git at https://www.github.com/google/edash-packager. We use gclient tool from Chromium to manage third_party libraries. You will need Git and Subversion (for third_party libraries) installed on your machine to access the source code.

2. Pull gclient and ninja from Chrome Depot Tools:
```Shell
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```
Add depot_tools to your PATH, for example, in linux:
```Shell
$ export PATH="$PATH":`pwd`/depot_tools
```
You may want to add this to your .bashrc file or your shell's equivalent so that you don’t need to reset your $PATH manually each time you open a new shell.

3. Get the source
```Shell
mkdir packager
cd packager
gclient config https://www.github.com/google/edash-packager.git --name=src
gclient sync
```

4. Build
We use ninja to build our code:
```
ninja -C src/out/{Debug/Release} [Module]
```
Module is optional. If not specified, build all, e.g.
```
ninja -C src/out/Debug                     # build all modules in Debug mode
ninja -C src/out/Release                   # build all modules in Release mode
ninja -C src/out/Release mp4               # build mp4 module in Debug mode
```
Refer to ninja manual for details.<br>
We also provide a mechanism to change build configurations, for example, developers can change build system to “make” by overriding GYP_GENERATORS using gyp_packager.py script, i.e.
```Shell
GYP_GENERATORS='make' src/gyp_packager.py
```
Another example, developers can also enable clang by overriding GYP_DEFINE.
```Shell
GYP_DEFINES='clang=1' src/gyp_packager.py
```
Take note that clang needs to be setup for the first time if it is not setup yet.
```Shell
src/tools/clang/scripts/update.sh
```

#Design overview#

Major modules are described below:

Demuxer is responsible for extracting elementary stream samples from a multimedia file, e.g. an ISO BMFF file. The demuxed streams can be fed into a muxer to generate multimedia files. For encrypted media, the user should implement the abstract DecryptorSource interface, which is used by Demuxer to decrypt the streams. The concrete DecryptorSource implementation wraps the fetching and management of decryption keys.

Demuxer reads from source through the File interface. A concrete LocalFile class is already implemented. The user may also implements his own File class if they want to read/write using a different kinds of protocol, e.g. network storage, http etc.

Muxer is responsible for taking elementary stream samples and producing media segments. An optional EncryptorSource can be provided to Muxer to generate encrypted outputs. Muxer writes to output using the same File interface as Demuxer.

Demuxer and Muxer are connected using MediaStream. MediaStream wraps the elementary streams and is responsible for the interaction between Demuxer and Muxer. A demuxer can transmits multiple MediaStreams; similarly, A muxer is able to accept and mux multiple MediaStreams, not necessarily from the same Demuxer.

MPDBuilder is responsible for the creation of Media Presentation Description as specified in ISO/IEC 23009-1 DASH MPD spec.

The only source format supported for now is ISO BMFF; the only output format supported right now is fragmented ISO BMFF with CENC. Support for more formats will be added soon.

##Creating DecryptorSource##

Not implemented yet. Not needed for now.

##Creating Demuxer##

```C++
  // Create a demuxer from |input_media_file|. At this moment we support
  // MP4 (ISO BMFF) only.
  // |input_media_file| could be any supported files, e.g. if the user has
  // implemented a network file interface with prefix “network”,
  // |input_media_file| with value “network://xxxx” would open a network
  // file automatically.
  // |decryptor_source| should be NULL if the input media is not encrypted.
  Demuxer demuxer(input_media_file, decryptor_source);
```

##Creating MpdBuilder##

```C++
MpdBuilder mpd_builder(MpdBuiler::kStatic);   // kStatic profile for VOD.
Using MpdBuilder Instance
mpd_builder.AddBaseUrl(“http://foo.com/bar”);  // Add a <BaseURL> element.
AdaptationSet* video_adaptation_set = mpd_builder.AddAdaptationSet();

// Create MediaInfo object.
Representation* representation =
    video_adaptation_set->AddRepresentation(media_info_object);
assert(representation);  // Make sure it succeeded.
// |representation| is owned by |video_adatptation_set|, no additional
// operations are required to generate a valid MPD.

std::cout << mpd_builder.ToString() << std::endl;  // Print the MPD to stdout.
MpdWriter: MpdBuilder Wrapper class
// Get file names with MediaInfo protobuf, i.e. media_info_files (array).
MpdWrite mpd_writer;
for (size_t i = 0; i < n; ++i)
  mpd_writer.AddFile(media_info_files[i]);

mpd_writer.WriteMpdToFile(“output_file_name.mpd”);
```

##Creating Muxer##

```C++
  // See below for muxer options.
  MuxerOptions options;

  // Create a MP4Muxer with options specified by |options|.
  mp4::MP4Muxer muxer(options);
```

###Muxer Options:###

```C++
  // Generate a single segment for each media presentation. This option
  // should be set for on demand profile.
  options.single_segment = true;

  // Segment duration in seconds. If single_segment is specified, this parameter
  // sets the duration of a subsegment; otherwise, this parameter sets the
  // duration of a segment. A segment can contain one to many fragments.
  options.segment_duration = 10.0;

  // Fragment duration in seconds. Should not be larger than the segment
  // duration.
  options.fragment_duration = 2.0;

  // Force segments to begin with stream access points. Segment duration may
  // not be exactly what asked by segment_duration.
  options.segment_sap_aligned = true;

  // Force fragments to begin with stream access points. Fragment duration
  // may not be exactly what asked by segment_duration. Imply
  // segment_sap_aligned.
  options.fragment_sap_aligned = true;

  // Set to true to normalize the presentation timestamps to start from zero.
  options.normalize_presentation_timestamp = true;

  // For ISO BMFF only.
  // Set the number of subsegments in each SIDX box. If 0, a single SIDX box
  // is used per segment. If -1, no SIDX box is used. Otherwise, the Muxer
  // will pack N subsegments in the root SIDX of the segment, with
  // segment_duration/N/fragment_duration fragments per subsegment.
  options.num_subsegments_per_sidx = 1;

  // Output file name. If segment_template is not specified, the Muxer
  // generates this single output file with all segments concatenated;
  // Otherwise, it specifies the init segment name.
  options.output_file_name = …;

  // Specify output segment name pattern for generated segments. It can
  // furthermore be configured by using a subset of the SegmentTemplate
  // identifiers: $RepresentationID$, $Number$, $Bandwidth$ and $Time.
  // Optional.
  options.segment_template = …;

  // Specify the temporary file for on demand media file creation.
  options.temp_file_name = …;
```

##Creating EncryptorSource##

```C++
// An EncryptorSource is optional. The stream won’t be encrypted if an
// EncryptorSource is not provided.
// There are several different EncryptorSource implementations.
```

###FixedEncryptorSource###

```C++
// Users can use FixedEncryptorSource if they have encryption keys already.
scoped_ptr<EncryptorSource> encryptor_source(
        new FixedEncryptorSource(key_id, key, pssh));
```

###WidevineEncryptorSource###

```C++
// Users may also use WidevineEncryptorSource to fetch keys from Widevine
// common encryption server.

// A request signer is required to sign the common encryption request.
scoped_ptr<RequestSigner> signer(
        RsaRequestSigner::CreateSigner(signer, pkcs1_rsa_private_key));
if (!signer) { … }

scoped_ptr<EncryptorSource> encryptor_source(new WidevineEncryptorSource(
        server_url, content_id, track_type, signer.Pass()));
```

After creating the encryptor source,

```C++
// Intialize encryptor source.
status = encryptor_source->Initialize();
if (!status.ok()) { … }

// Set encryptor source to muxer.
muxer->SetEncryptorSource(encryptor_source.get(), clear_lead);
```

##Connecting Demuxer and Muxer##

```C++
  // Initialize the demuxer.
  status = demuxer.Initialize();
  if (!status.ok()) { … }

  // After intializing the demuxer, we can query demuxer streams
  // using demuxer.streams().
  // The function below adds the first stream into muxer, which sets up
  // the connection between Demuxer and Muxer.
  status = muxer.AddStream(demuxer.streams()[0]);
  if (!status.ok()) { … }
```

##Starting Remuxing##

```C++
  // Starts remuxing process. It runs until completion or abort due to error.
  status = demuxer.Run();
  if (!status.ok()) { … }
```

#Packager API detail#

Doxygen API documentation coming soon.

#Driver Program Sample Usage#

Sample driver programs packager_main and mpd_generator are written using the SDK. Some sample usages:

Run the program without arguments will display the help page with the list of command line arguments:
```Shell
packager_main
```

Dump stream info:
```Shell
packager_main sintel.mp4 --dump_stream_info
```

Demux audio from the input and generate a fragmented mp4:
```Shell
packager_main sintel.mp4 --stream audio --output "fragmented_sintel.mp4"
```

Demux the first stream from the input and generate a fragmented mp4:
```Shell
packager_main sintel.mp4 --stream 0 --output "fragmented_sintel.mp4"
```

Demux video from the input and generate an encrypted fragmented mp4 using widevine encryption with RSA signing key file "widevine_test_private.der":
```Shell
packager_main sintel.mp4 \
--stream video \
--output "encrypted_sintel.mp4" \
--enable_widevine_encryption \
--server_url "license.uat.widevine.com/cenc/getcontentkey/widevine_test" \
--content_id "content_sintel" \
--signer "widevine_test" \
--rsa_signing_key_path "widevine_test_private.der"
```

The program also supports AES signing.
```Shell
packager_main sintel.mp4 \
--stream video \
--output "encrypted_sintel.mp4" \
--enable_widevine_encryption \
--server_url "license.uat.widevine.com/cenc/getcontentkey/widevine_test" \
--content_id "content_sintel" \
--signer "widevine_test" \
--aes_signing_key "1ae8ccd0e7985cc0b6203a55855a1034afc252980e970ca90e5202689f947ab9" \
--aes_signing_iv "d58ce954203b7c9a9a9d467f59839249"
```

By default, packager_main will output MediaInfo files. “.media_info” file extension is appended to the output media files. The MediaInfo files can be passed to mpd_generator to generate an MPD. mpd_generator uses MpdWriter.
```Shell
mpd_generator \
--base_urls "http://foo.com/bar1,http://foo.com/bar2" \
--input "encrypted_sintel.mp4.media_info" \
--output "example.mpd"
```

