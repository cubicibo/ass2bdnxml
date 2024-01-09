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
| ``--video-format`` | Choices: 1080p, 1080i, 720p, 576p, 576i, 480p, 480i.   |
|                    | Default: ``1080p``                                     |
+--------------------+--------------------------------------------------------+
| ``-f``             | Sets the video frame rate.                             |
| ``--fps``          | Values: 23.976, 24, 25, 29.97, 50, 59.94, 60 (UHD only)|
|                    | Default: ``23.976``                                    |
+--------------------+--------------------------------------------------------+
| ``-q``             | Sets and enable image quantization with N colors.      |
| ``--quantize``     | Value in [0; 256] inc. Default: ``0`` (32bit RGBA PNGs)|
|                    | **Notes: DO NOT USE if target is SUPer.**              |
|                    | This must be enabled if target software is Scenarist BD|
+--------------------+--------------------------------------------------------+
| ``-a``             | Sets an additional font directory for custom fonts not |
| ``--fontdir``      | embedded in the ASS or provided by the OS font manager.|
+--------------------+--------------------------------------------------------+
| ``-s``             | Sets the event split across 2 graphics behaviour.      |
| ``--split``        | 0: Disabled, 1: Normal, 2: Strong, 3: Very aggressive. |
|                    | Default: ``0`` (Disabled)                              |
|                    | **Note: DO NOT USE if target is SUPer.**               |
+--------------------+--------------------------------------------------------+
| ``-m``             | Sets the vertical and opt. horizontal margins to split |
| ``--splitmargin``  | Format: ``VxH`` (V=y difference, H=x difference).      |
|                    | Default: 0x0. Split search is done on 8x8 grid anyway. |
|                    | Note: If only V is given, 'x' separator must be omitted|
+--------------------+--------------------------------------------------------+
| ``-h``             | Flag to squeeze bitmaps to correct aspect ratio. Needed|
| ``--anamorphic``   | for SD anamorphic content else subs will be stretched. |
+--------------------+--------------------------------------------------------+
| ``-z``             | Flag to correct the square pixel horizontal stretch    |
| ``--squarepx``     | with SD 4:3 content. Use ``--anamorphic`` for 16:9 SD. |
+--------------------+--------------------------------------------------------+
| ``-u``             | Do 4:3 rendering for 16:9 container (e.g 1440x1080     |
| ``--fullscreen``   | pillarboxed to 1920x1080). Recommended for ASS made    |
|                    | with the 4/3 geometry clip rather than pillarboxed one.|
+--------------------+--------------------------------------------------------+
| ``-p``             | Set custom pixel aspect ratio in rendering.            |
| ``--par``          | Format: floating point or fraction like ``852:720``.   |
+--------------------+--------------------------------------------------------+
| ``-o``             | Sets the TC offset to shift all of the BDN Timecodes.  |
| ``--offset``       | Default: ``00:00:00:00`` (offset of zero frame)        |
|                    | Note: TC string must be the standard SMPTE NDF format. |
+--------------------+--------------------------------------------------------+
| ``-r``             | Flag to encode PNGs without using palette entry zero   |
| ``--rleopt``       | Can prevent RLE/line width encoding errors at authoring|
|                    | Ignored if 32-bit RGBA output (``--quantize`` unused). |
+--------------------+--------------------------------------------------------+
| ``-c``             | Flag to name the output XML according to the input ASS |
| ``--copyname``     | file. The input ASS file must have a valid extension.  |
+--------------------+--------------------------------------------------------+
| ``-t``             | Sets the human-readable name of the subtitle track.    |
| ``--trackname``    | Default: ``Undefined``                                 |
+--------------------+--------------------------------------------------------+
| ``-l``             | Sets the language of the subtitle track.               |
| ``--language``     | Default: ``und``                                       |
+--------------------+--------------------------------------------------------+
| ``-w``             | Sets the ASS event output width, defaults to BDN width.|
| ``--width-render`` | Equal to the squeezed width for SD anamorphic as the   |
|                    | player will unsqueeze. Prefer ``-h`` if possible.      |
+--------------------+--------------------------------------------------------+
| ``-x``             | Sets the ASS storage width, defaults to BDN width.     |
| ``--width-store``  | Equals unsqueezed width for SD anamorphic.             |
|                    | Prefer ``-h`` if possible.                             |
+--------------------+--------------------------------------------------------+

The naming scheme for ``--width-render`` and ``--width-store`` with respect to the expected values may
seem counterintuitive but it is logical. This is to configure libass to do the inverse transform of
the anamorphic stretch, so subtitles appear normally when the Blu-ray players stretch them to widescreen.
However, ``--anamorphic`` should do the magic and you should only ever use those for non-standard files.

Below are parameters to tune libimagequant (LIQ). Those shall only be used along ``--quantize`` (``-q``). Only long parameters names are available.

+--------------------+--------------------------------------------------------+
| Option             | Effect                                                 |
+====================+========================================================+
| ``--liq-speed``    | LIQ speed. Lower value are slower but higher quality.  |
|                    | Choices: value within [1; 10] incl. Default: ``4``     |
+--------------------+--------------------------------------------------------+
| ``--liq-quality``  | Quantization quality. Values < 100 can be visually     |
|                    | lossless and will compress way better as PGS.          |
|                    | Default: ``100`` but 90~97 is recommended in general.  |
+--------------------+--------------------------------------------------------+
| ``--liq-dither``   | Dithering level, value must be within [0; 1.0] incl.   |
|                    | Default: ``1.0``. Disable: ``0``. LIQ dithering is soft|
|                    | so default or ``0.5`` is perfect in general.           |
+--------------------+--------------------------------------------------------+

Moreover, the last table has debugging parameters. These should not have any practical in most scenarios.

+--------------------+--------------------------------------------------------+
| Option             | Effect                                                 |
+====================+========================================================+
| ``--height-store`` | Sets the ASS storage height. Only useful for ASS files |
|                    | with complex transforms and unusual video height.      |
+--------------------+--------------------------------------------------------+
| ``--render-height``| Sets the height to use as output ASS frame.            |
|                    | Defaults to BDN output height if unspecified.          |
+--------------------+--------------------------------------------------------+
| ``--dvd-mode``     | Flag to apply a contrast change that may improve       |
|                    | subtitle appearance with the limited resolution and    |
|                    | color palette of DVD subtitles.                        |
+--------------------+--------------------------------------------------------+
| ``--keep-dupes``   | Flag to not merge events that are reported as different|
|                    | by libass yet identical when composited (e.g ASSDraw). |
+--------------------+--------------------------------------------------------+
| ``--negative``     | Flag to indicate a negative ``--offset``.              |
|                    | Ignored if no ``--offset`` provided.                   |
+--------------------+--------------------------------------------------------+
| ``--hinting``      | Flag to enable soft hinting in libass.                 |
+--------------------+--------------------------------------------------------+

Notes
-----

- 480p and 576p shall only be used to produce captions for secondary video streams, not primary.
- Real 60 fps is only supported on the UHD BD format.
- Captions for 4K UHD BDs are always rendered at 1080p. The players upscale the presentation graphics on playback.
- For Blu-ray, 59.94 is seemingly only for 720p59.94 content. 1080i59.94 should use 29.97, but there may be some leeway and it does not appear to be enforced.
