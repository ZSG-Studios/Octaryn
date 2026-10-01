# Zorah Geometry Export

Version 2.0

This is a glTF 2.0 export of the highly detailed raw geometry from the [NVIDIA RTX Kit - Zorah Sample](https://developer.nvidia.com/rtx-kit) as presented at [GDC 2025](https://developer.nvidia.com/blog/nvidia-rtx-advances-with-neural-rendering-and-digital-human-technologies-at-gdc-2025/).

The primary purpose of this file is to be used with the [vk_lod_clusters](https://github.com/nvpro-samples/vk_lod_clusters)
Vulkan sample. Pass one of the two `.cfg` files to the Vulkan sample.

Uses [EXT_meshopt_compression](https://meshoptimizer.org/#mesh-compression) for smaller file size.

For easier deployment the scene was kept as single binary file. We recommend
splitting this up into individual per-mesh files for processing. This can be
done quite nicely using Python or other scripting languages by parsing the
gltf as json file.

Most importers will fail handling the very large file, especially `.bin` files
exceeding 4 GiB.

Compared to the original demo some vegetation had to be removed to allow
distribution.

When experimenting with ray tracing performance, we recommend to use
the `no_mountains.cfg` file.
