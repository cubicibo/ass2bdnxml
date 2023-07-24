Convert ASS subtitles into BDN XML + PNG images
===============================================

ass2bdnxml is a command line software originally written by `mia0 <https://github.com/mia-0>`_  to convert .ASS to BDN XML + PNG assets.
The generated files can then be imported in Blu-Ray authoring softwares or used in `SUPer <https://github.com/cubicibo/SUPer>`_ to generate raw PGS stream files, usable in software like tsMuxer.
This fork enables additional options revolving around libass to support more complex ASS files. It also fixes alpha blending and features optional event splitting across two graphics to reduce buffer usage when the BDNXML is directly imported in the authoring software.

Building
--------

You can either use Meson (see `the Meson documentation <https://mesonbuild.com/>`_)::

    meson builddir
    ninja -C builddir

Or you can build it without using a build system::

    cc *.c -o ass2bdnxml $(pkg-config --cflags --libs libass) $(pkg-config --cflags --libs png) -lm

(Depending on your platform, you may have to omit ``-lm`` and replace ``png`` by ``libpng``)

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
| ``-h``             | Specify the .ass height to use as frame space          |
| ``--render-height``| The output height is used if not specified. Some ass   |
|                    | tags may not render properly if the value is improper. |
+--------------------+--------------------------------------------------------+
| ``-g``             | libass hinting will be enabled if provided.            |
| ``--hinting``      |                                                        |
+--------------------+--------------------------------------------------------+
| ``-p``             | Set the pixel aspect ratio in libass. Required for     |
| ``--par``          | anamorphic content. Defaults to libass default value.  |
+--------------------+--------------------------------------------------------+
| ``-x``             | ASS storage width to use in libass. Typically the      |
| ``--width-store``  | before-anamorphic width. -p should be preferred, over  |
|                    | setting the storage space. Last resort option generally|
+--------------------+--------------------------------------------------------+
| ``-y``             | ASS storage height to use in libass. Should be left    |
| ``--height-store`` | as default unless the video format is non-standard.    |
+--------------------+--------------------------------------------------------+
| ``-a``             | Specify an additional font directory for custom fonts  |
| ``--fontdir``      | not in the OS font manager or embedded in the ASS.     |
+--------------------+--------------------------------------------------------+
| ``-s``             | Whenever possible, split an event in two graphic       |
| ``--split``        | objects. This may reduce the buffer usage.             |
|                    | Should NOT be used if the BDNXML is generated for SUPer|
+--------------------+--------------------------------------------------------+
