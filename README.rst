Convert ASS subtitles into BDN XML + PNG images
===============================================

ass2bdnxml is a command line software originally written by `mia-0 <https://github.com/mia-0>`_  to convert .ASS to BDN XML + PNG assets.

The generated files can then be imported in Blu-Ray authoring softwares or used in `SUPer <https://github.com/cubicibo/SUPer>`_ to generate Presentation Graphic Streams (PGS) files, usable in software like tsMuxer.
This fork enables advanced libass features like embedded fonts. It fixes alpha blending, improves time handling, adds event splitting across two graphics and image quantization via libimagequant.

Building
--------

You can either use Meson (see `the Meson documentation <https://mesonbuild.com/>`_)::

    meson builddir
    ninja -C builddir

Or you can build it without using a build system::

    cc *.c -o ass2bdnxml $(pkg-config --cflags --libs libass) $(pkg-config --cflags --libs libpng) $(pkg-config --cflags --libs imagequant) -lm

(Depending on your platform, you may have to omit ``-lm`` and replace ``libpng`` by ``png``)

Usage
-----

``ass2bdnxml`` will write the output bdn.xml and PNGs to the current working directory.
Simply invoke it like this::

    ass2bdnxml [OPTIONS] PATH_TO_FILE/subs.ass

The following optional arguments are available:

+--------------------+--------------------------------------------------------+
| Option             | Effect                                                 |
+====================+========================================================+
| ``-v``             | Sets the video format to render subtitles in.          |
| ``--video-format`` | Choices: 1080p, 1080i, 720p, 576i, 480p, 480i.         |
|                    | Default: ``1080p``                                     |
+--------------------+--------------------------------------------------------+
| ``-f``             | Sets the video frame rate.                             |
| ``--fps``          | Choices: 23.976, 24, 25, 29.97, 50, 59.94.             |
|                    | Default: ``23.976``                                    |
+--------------------+--------------------------------------------------------+
| ``-q``             | Sets and enable image quantization with N colors.      |
| ``--quantize``     | Choices: value in [0; 255].                            |
|                    | Default: ``0`` (Disabled, PNGs are 32-bit RGBA)        |
|                    | Notes: Do not use if the BDNXML is generated for SUPer.|
|                    | This must be enabled if target software is Scenarist BD|
+--------------------+--------------------------------------------------------+
| ``-a``             | Sets an additional font directory for custom fonts not |
| ``--fontdir``      | embedded in the ASS or provided by the OS font manager.|
+--------------------+--------------------------------------------------------+
| ``-s``             | Sets the event split across 2 graphics behaviour.      |
| ``--split``        | 0: Disabled, 1: Normal, 2: Strong, 3: Very aggressive. |
|                    | Default: ``0`` (Disabled)                              |
|                    | Note: Do not use if the BDNXML is generated for SUPer. |
+--------------------+--------------------------------------------------------+
| ``-m``             | Sets the vertical and opt. horizontal margins to split |
| ``--splitmargin``  | Format: ``VxH`` (V=y difference, H=x difference).      |
|                    | Default: 0x0. Split search is done on 8x8 grid anyway. |
|                    | Note: If only V is given, 'x' separator must be omitted|
+--------------------+--------------------------------------------------------+
| ``-p``             | Sets the ASS pixel aspect ratio. Required for          |
| ``--par``          | anamorphic content. Defaults to libass default value.  |
+--------------------+--------------------------------------------------------+
| ``-o``             | Sets the TC offset to shift all of the BDN Timecodes.  |
| ``--offset``       | Default: ``00:00:00:00`` (offset of zero frame)        |
|                    | Note: TC string must be the standard SMPTE NDF format. |
+--------------------+--------------------------------------------------------+
| ``-z``             | Flag to indicate a negative ``--offset``.              |
| ``--negative``     | Ignored if no offset is provided.                      |
+--------------------+--------------------------------------------------------+
| ``-r``             | Flag to encode PNGs without using palette entry zero   |
| ``--rleopt``       | Can maybe prevent RLE/line width errors in authorign.  |
|                    | Ignored if 32-bit RGBA output (``--quantize`` unused). |
+--------------------+--------------------------------------------------------+
| ``-t``             | Sets the human-readable name of the subtitle track.    |
| ``--trackname``    | Default: ``Undefined``                                 |
+--------------------+--------------------------------------------------------+
| ``-l``             | Sets the language of the subtitle track.               |
| ``--language``     | Default: ``und``                                       |
+--------------------+--------------------------------------------------------+
| ``-d``             | Flag to apply a contrast change that may improve       |
| ``--dvd-mode``     | subtitle appearance with the limited resolution and    |
|                    | color palette of DVD subtitles.                        |
+--------------------+--------------------------------------------------------+
| ``-w``             | Sets the width to use as ASS frame & storage space     |
| ``--render-width`` | Defaults to output width if not specified. Some ass    |
|                    | tags may not render properly if the value is improper. |
+--------------------+--------------------------------------------------------+
| ``-h``             | Sets the height to use as ASS frame & storage space    |
| ``--render-height``| Defaults to output height if not specified. Some ass   |
|                    | tags may not render properly if the value is improper. |
+--------------------+--------------------------------------------------------+
| ``-g``             | Flag to enable libass hinting.                         |
| ``--hinting``      |                                                        |
+--------------------+--------------------------------------------------------+
| ``-x``             | Sets the ASS storage width. I.e the pre-anamorphic     |
| ``--width-store``  | width. ``-p`` should be preferred, Last resort option. |
+--------------------+--------------------------------------------------------+
| ``-y``             | Sets the ASS storage height. Last resort option for    |
| ``--height-store`` | ASS with complex transforms with unusual video height. |
+--------------------+--------------------------------------------------------+
