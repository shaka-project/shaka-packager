Shaka Packager Library
======================

Documentation for the top level Shaka packager library. See
`Internal API <https://google.github.io/shaka-packager/docs/annotated.html>`_
for documentation on internal APIs.

.. doxygenclass:: shaka::Packager

Sample code:

.. code-block:: c++

    shaka::Packager packager;

    // Setup packaging parameters.
    shaka::PackagingParams packaging_params;
    // Use default parameters here.

    // Setup stream descriptors.
    std::vector<shaka::StreamDescriptor> stream_descriptors;
    shaka::StreamDescriptor stream_descriptor;
    stream_descriptor.input = "input.mp4";
    stream_descriptor.stream_selector = "video";
    stream_descriptor.output = "output_video.mp4";
    stream_descriptors.push_back(stream_descriptor);
    shaka::StreamDescriptor stream_descriptor;
    stream_descriptor.input = "input.mp4";
    stream_descriptor.stream_selector = "audio";
    stream_descriptor.output = "output_audio.mp4";
    stream_descriptors.push_back(stream_descriptor);

    shaka::Status status = packager.Initialize(packaging_params,
                                               stream_descriptors);
    if (!status.ok()) { ... }
    status = packager.Run();
    if (!status.ok()) { ... }
