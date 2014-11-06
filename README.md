Media packaging SDK intended for C++ programmers writing DASH packager applications with common encryption support, Widevine DRM support, Live, and Video-On-Demand.

This document provides the information needed to create a DASH packager that is able to remux and encrypt a video into fragmented ISO BMFF format with common encryption (CENC) support. The DASH packaging API is also designed in such a way for easy extension to more source and destination formats.

# Setting up for development #

1. Packager source is managed by Git at https://www.github.com/google/edash-packager. We use gclient tool from Chromium to manage third party libraries. You will need Git (v1.7.5 or above) and Subversion (for third party libraries) installed on your machine to access the source code.

2. Install Chromium depot tools which contains gclient and ninja

  See http://www.chromium.org/developers/how-tos/install-depot-tools for details.

3. Get the source

  ```Shell
  mkdir edash_packager
  cd edash_packager
  gclient config https://www.github.com/google/edash-packager.git --name=src
  gclient sync
  ```

4. Build

  We use ninja, which is much faster than make, to build our code:
  ```Shell
  cd src
  ninja -C out/{Debug,Release} [Module]
  ```
  Module is optional. If not specified, build all, e.g.
  ```Shell
  ninja -C out/Debug                     # build all modules in Debug mode
  ninja -C out/Release                   # build all modules in Release mode
  ninja -C out/Release mp4               # build mp4 module in Release mode
  ```
  Refer to ninja manual for details.

  We also provide a mechanism to change build configurations, for example, developers can change build system to “make” by overriding *GYP_GENERATORS*.
  ```Shell
  GYP_GENERATORS='make' gclient runhooks
  ```
  Another example, developers can also enable clang by overriding *GYP_DEFINE*.
  ```Shell
  GYP_DEFINES='clang=1' gclient runhooks
  ```

5. Updating the code

  Update your current branch with *git pull* followed by *gclient sync*. Note that if you are not on a branch, *git pull* will not work, and you will need to use *git fetch* instead.

6. Contributing

  See https://github.com/google/edash-packager/blob/master/CONTRIBUTING.md for details.


#Design overview#

Major modules are described below:

Demuxer is responsible for extracting elementary stream samples from a multimedia file, e.g. an ISO BMFF file. The demuxed streams can be fed into a muxer to generate multimedia files. An optional KeySource can be provided to Demuxer to decrypt CENC and WVM source content.

Demuxer reads from source through the File interface. A concrete LocalFile class is already implemented. The users may also implement their own File class if they want to read/write using a different kinds of protocol, e.g. network storage, http etc.

Muxer is responsible for taking elementary stream samples and producing media segments. An optional KeySource can be provided to Muxer to generate encrypted outputs. Muxer writes to output using the same File interface as Demuxer.

Demuxer and Muxer are connected using MediaStream. MediaStream wraps the elementary streams and is responsible for the interaction between Demuxer and Muxer. A demuxer can transmits multiple MediaStreams; similarly, A muxer is able to accept and mux multiple MediaStreams, not necessarily from the same Demuxer.

MpdBuilder is responsible for the creation of Media Presentation Description as specified in ISO/IEC 23009-1 DASH MPD spec.

Supported source formats: ISO BMFF (both fragmented and non-fragmented), MPEG-2 TS, IPTV (MPEG-2 TS over UDP), and WVM (Widevine); the only output format supported currently is fragmented ISO BMFF with CENC. Support for more formats will be added soon.


##Creating Demuxer##

```C++
// Create a demuxer from |input_media_file|.
// |input_media_file| could be any supported files, e.g. if the users have
// implemented a network file interface with prefix “network”,
// |input_media_file| with value “network://xxxx” would open a network
// file automatically.
Demuxer demuxer(input_media_file);
```

##Creating KeySource for source content decryption##

```C++
// A KeySource is required if the source content is encrypted, since the media
// must be decrytped prior to further processing.
```

###WidevineKeySource###

```C++
// Users may use WidevineKeySource to fetch keys from Widevine
// common encryption server.

scoped_ptr<WidevineKeySource> widevine_decryption_key_source(
    new WidevineKeySource(key_server_url));

// A request signer might be required to sign the common encryption request.
scoped_ptr<RequestSigner> signer(
    RsaRequestSigner::CreateSigner(signer_name, pkcs1_rsa_private_key));
if (!signer) { … }
widevine_decryption_key_source->set_signer(signer.Pass());

// Set encryption key source to demuxer.
muxer->SetKeySource(widevine_decryption_key_source.Pass());
```


##Creating MpdBuilder##

```C++
// |mpd_type| indicates whether the mpd should be for VOD or live profile.
// |mpd_options| contains a set of configurable options. See below for details.
MpdBuilder mpd_builder(mpd_type, mpd_options);
```

Mpd Options
```C++
// Specifies, in seconds, a common duration used in the definition of the MPD
// Representation data rate.
mpd_options.min_buffer_time = 5.0;

// The below options are for live profile only.

// Offset with respect to the wall clock time for MPD availabilityStartTime
// and availabilityEndTime values, in seconds.
mpd_options.availability_time_offset = 10.0;

// Indicates to the player how often to refresh the media presentations, in seconds.
mpd_options.minimum_update_period = 5.0;

// Guranteed duration of the time shifting buffer, in seconds.
mpd_options.time_shift_buffer_depth = 1800.0;

// Specifies a delay, in seconds, to be added to the media presentation time.
mpd_options.suggested_presentation_delay = 0.0;
```

Using MpdBuilder Instance
```C++
mpd_builder.AddBaseUrl("http://foo.com/bar");  // Add a <BaseURL> element.
AdaptationSet* video_adaptation_set = mpd_builder.AddAdaptationSet();

// Create MediaInfo object.
Representation* representation =
    video_adaptation_set->AddRepresentation(media_info_object);
assert(representation);  // Make sure it succeeded.
// |representation| is owned by |video_adatptation_set|, no additional
// operations are required to generate a valid MPD.

std::cout << mpd_builder.ToString() << std::endl;  // Print the MPD to stdout.
```

MpdWriter: MpdBuilder Wrapper class
```C++
// Get file names with MediaInfo protobuf, i.e. media_info_files (array).
MpdWrite mpd_writer;
for (size_t i = 0; i < n; ++i)
  mpd_writer.AddFile(media_info_files[i]);

mpd_writer.WriteMpdToFile("output_file_name.mpd");
```

##Creating Muxer##

```C++
// See below for muxer options.
MuxerOptions muxer_options;

// Create a MP4Muxer with options specified by |options|.
mp4::MP4Muxer muxer(muxer_options);
```

Muxer Options
```C++
// Generate a single segment for each media presentation. This option
// should be set for on demand profile.
muxer_options.single_segment = true;

// Segment duration in seconds. If single_segment is specified, this parameter
// sets the duration of a subsegment; otherwise, this parameter sets the
// duration of a segment. A segment can contain one to many fragments.
muxer_options.segment_duration = 10.0;

// Fragment duration in seconds. Should not be larger than the segment
// duration.
muxer_options.fragment_duration = 2.0;

// Force segments to begin with stream access points. Segment duration may
// not be exactly what asked by segment_duration.
muxer_options.segment_sap_aligned = true;

// Force fragments to begin with stream access points. Fragment duration
// may not be exactly what asked by segment_duration. Imply
// segment_sap_aligned.
muxer_options.fragment_sap_aligned = true;

// For ISO BMFF only.
// Set the number of subsegments in each SIDX box. If 0, a single SIDX box
// is used per segment. If -1, no SIDX box is used. Otherwise, the Muxer
// will pack N subsegments in the root SIDX of the segment, with
// segment_duration/N/fragment_duration fragments per subsegment.
muxer_options.num_subsegments_per_sidx = 1;

// Output file name. If segment_template is not specified, the Muxer
// generates this single output file with all segments concatenated;
// Otherwise, it specifies the init segment name.
muxer_options.output_file_name = …;

// Specify output segment name pattern for generated segments. It can
// furthermore be configured by using a subset of the SegmentTemplate
// identifiers: $RepresentationID$, $Number$, $Bandwidth$ and $Time$.
// Optional.
muxer_options.segment_template = …;

// Specify the temporary directory for intermediate files.
muxer_options.temp_dir = …;

// User specified bit rate for the media stream. If zero, the muxer will
// attempt to estimate.
muxer_options.bandwidth = 0;
```

##Creating KeySource for content encryption##

```C++
// A KeySource is optional. The stream won’t be encrypted if an
// KeySource is not provided.
```

###WidevineKeySource###

```C++
// Users may use WidevineKeySource to fetch keys from Widevine
// common encryption server.

scoped_ptr<WidevineKeySource> widevine_encryption_key_source(
    new WidevineKeySource(key_server_url, signer.Pass()));

// A request signer might be required to sign the common encryption request.
scoped_ptr<RequestSigner> signer(
    RsaRequestSigner::CreateSigner(signer_name, pkcs1_rsa_private_key));
if (!signer) { … }
widevine_encryption_key_source->set_signer(signer.Pass());

// Grab keys for the content.
status = widevine_encryption_key_source->FetchKeys(content_id, policy));
if (!status.ok()) { … }

// Set encryption key source to muxer.
// |max_sd_pixels| defines the threshold to determine whether a video track
// should be considered as SD or HD and accordingly, whether SD key or HD key
// should be used to encrypt the stream.
// |clear_lead| specifies clear lead duration in seconds.
// |crypto_period_duration| if not zero, enable key rotation with specified
// crypto period.
muxer->SetKeySource(
    widevine_encryption_key_source.get(), max_sd_pixels,
    clear_lead, crypto_period_duration);
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

Sample driver programs **packager** and **mpd_generator** are written using the SDK.

Some sample usages:

Run the program without arguments will display the help page with the list of command line arguments:
```Shell
packager
```

Dump stream info:
```Shell
packager input=sintel.mp4 --dump_stream_info
```

Demux audio from the input and generate a fragmented mp4:
```Shell
packager input=sintel.mp4,stream=audio,output=fragmented_sintel.mp4
```

Demux streams from the input and generates a mpd with on-demand profile along with fragmented mp4:
```Shell
packager \
  input=sintel.mp4,stream=audio,output=sintel_audio.mp4 \
  input=sintel.mp4,stream=video,output=sintel_video.mp4 \
--profile on-demand \
--mpd_output sintel_vod.mpd
```

You may also generate mpd with live profile. Here is an example with IPTV input streams:
```Shell
packager \
  'input=udp://224.1.1.5:5003,stream=audio,init_segment=live-audio.mp4,segment_template=live-audio-$Number$.mp4,bandwidth=130000'  \
  'input=udp://224.1.1.5:5003,stream=video,init_segment=live-video-sd.mp4,segment_template=live-video-sd-$Number$.mp4,bandwidth=2000000' \
  'input=udp://224.1.1.5:5002,stream=video,init_segment=live-video-hd.mp4,segment_template=live-video-hd-$Number$.mp4,bandwidth=5000000' \
--profile live \
--mpd_output live.mpd
```

Demux video from the input and generate an encrypted fragmented mp4 using Widevine encryption with RSA signing key file *widevine_test_private.der*:
```Shell
packager input=sintel.mp4,stream=video,output=encrypted_sintel.mp4 \
--enable_widevine_encryption \
--key_server_url "https://license.uat.widevine.com/cenc/getcontentkey/widevine_test" \
--content_id "3031323334353637" \
--signer "widevine_test" \
--rsa_signing_key_path "widevine_test_private.der"
```

The program also supports AES signing. Here is an example with encryption key rotates every 1800 seconds:
```Shell
packager input=sintel.mp4,stream=video,output=encrypted_sintel.mp4 \
--enable_widevine_encryption \
--key_server_url "https://license.uat.widevine.com/cenc/getcontentkey/widevine_test" \
--content_id "3031323334353637" \
--signer "widevine_test" \
--aes_signing_key "1ae8ccd0e7985cc0b6203a55855a1034afc252980e970ca90e5202689f947ab9" \
--aes_signing_iv "d58ce954203b7c9a9a9d467f59839249" \
--crypto_period_duration 1800
```

Demux and decrypt video from a WVM container, and generate encrypted fragmented mp4 using Widevine encryption with RSA signing key file *widevine_test_private.der*:
```Shell
packager input=sintel.wvm,stream=video,output=encrypted_sintel.mp4 \
--enable_widevine_decryption \
--enable_widevine_encryption \
--key_server_url "https://license.uat.widevine.com/cenc/getcontentkey/widevine_test" \
--content_id "3031323334353637" \
--signer "widevine_test" \
--rsa_signing_key_path "widevine_test_private.der"
```

The program can be told to generate MediaInfo files, which can be fed to **mpd_generate** to generate the mpd file.
```Shell
packager \
  input=sintel.mp4,stream=video,output=sintel_video.mp4 \
  input=sintel.mp4,stream=audio,output=sintel_audio.mp4 \
--output_media_info

mpd_generator \
--input "sintel_video.mp4.media_info,sintel_audio.mp4.media_info" \
--output "sintel.mpd"
```
