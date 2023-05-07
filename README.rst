Convert ASS subtitles into BDN XML + PNG images
===============================================

This is a little utility I wrote back when I had to produce some Blu-ray
subtitles for a documentary. The generated files can be used by tools like
`BDSup2Sub++ <https://github.com/amichaelt/BDSup2SubPlusPlus>`_.

Building
--------

You can either use Meson (see `the Meson documentation <https://mesonbuild.com/>`_)::

    meson builddir
    ninja -C builddir

Or you can build it without using a build system::

    cc *.c -o ass2bdnxml $(pkg-config --cflags --libs libass) $(pkg-config --cflags --libs png) -lm

(Depending on your platform, you may have to omit ``-lm``)

Usage
-----

``ass2bdnxml`` will write its output to the current working directory.
Simply invoke it like this::

    ass2bdnxml ../subtitles.ass

The following optional arguments are available:

+--------------------+--------------------------------------------------------+
| Option             | Effect                                                 |
+====================+========================================================+
| ``-t``             | Sets the human-readable name of the subtitle track.    |
| ``--trackname``    | Default: ``Undefined``                                 |
+--------------------+--------------------------------------------------------+
| ``-l``             | Sets the language of the subtitle track.               |
| ``--language``     | Default: ``und``                                       |
+--------------------+--------------------------------------------------------+
| ``-v``             | Sets the video format to render subtitles in.          |
| ``--video-format`` | Choices: 1080p, 1080i, 720p, 576i, 480p, 480i          |
|                    | Default: 1080p                                         |
+--------------------+--------------------------------------------------------+
| ``-f``             | Sets the video frame rate.                             |
| ``--fps``          | Choices: 23.976, 24, 25, 29.97, 50, 59.94              |
|                    | Default: 23.976                                        |
+--------------------+--------------------------------------------------------+
| ``-d``             | Applies a contrast change that hopefully improves      |
| ``--dvd-mode``     | subtitle appearance with the limited resolution and    |
|                    | color palette of DVD subtitles.                        |
+--------------------+--------------------------------------------------------+
| ``-w``             | Specify the .ass width to use as frame & storage space |
| ``--render-width`` | The output width is used if not specified. Some ass    |
|                    | tags may not render properly if the value is improper. |
+--------------------+--------------------------------------------------------+
| ``-h``             | Specify the .ass height to use as frame & storage space|
| ``--render-height``| The output height is used if not specified. Some ass   |
|                    | tags may not render properly if the value is improper. |
+--------------------+--------------------------------------------------------+
| ``-g``             | libass hinting will be enabled if provided.            |
| ``--hinting``      |                                                        |
+--------------------+--------------------------------------------------------+
| ``-a``             | Set the pixel ratio to use in libass. Required for     |
| ``--ass-pixratio`` | anamorphic content. Defaults to libass default value.  |
+--------------------+--------------------------------------------------------+
