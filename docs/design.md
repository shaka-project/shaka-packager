#Design overview#

Major modules are described below:

Demuxer is responsible for extracting elementary stream samples from a multimedia file, e.g. an ISO BMFF file. The demuxed streams can be fed into a muxer to generate multimedia files. An optional KeySource can be provided to Demuxer to decrypt CENC and WVM source content.

Demuxer reads from source through the File interface. A concrete LocalFile class is already implemented. The users may also implement their own File class if they want to read/write using a different kinds of protocol, e.g. network storage, http etc.

Muxer is responsible for taking elementary stream samples and producing media segments. An optional KeySource can be provided to Muxer to generate encrypted outputs. Muxer writes to output using the same File interface as Demuxer.

Demuxer and Muxer are connected using MediaStream. MediaStream wraps the elementary streams and is responsible for the interaction between Demuxer and Muxer. A demuxer can transmits multiple MediaStreams; similarly, A muxer is able to accept and mux multiple MediaStreams, not necessarily from the same Demuxer.

MpdBuilder is responsible for the creation of Media Presentation Description as specified in ISO/IEC 23009-1 DASH MPD spec.

Supported source formats: ISO BMFF (both fragmented and non-fragmented), MPEG-2 TS, IPTV (MPEG-2 TS over UDP), and WVM (Widevine); the only output format supported currently is fragmented ISO BMFF with CENC. Support for more formats will be added soon.

API document is available at https://google.github.io/edash-packager/docs.

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
