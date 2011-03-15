KrazyRenderer
=============

There are a lot of good songs from a rhythm online game [Krazy Rain](http://www.krazyrain.com/),
but all of these songs are only playable in the game, and there are no OSTs released.
One may try to record directly from the game, but they must press the buttons very precisely for
the results to sound nice. Press one note offbeat, the whole recording fails.
Even the [autoplay](http://www.youtube.com/watch?v=Krc_VDVNcUk) can still get some sounds off
(you can hear some gaps between two sounds).

I would like to have some songs in my library, so I have to made this app to convert them.

It reads in the note data from the notes files (.xne and .xnt) and all samples (.wav) and creates a wave file (.wav)
that sounds like the in-game version, but it plays the notes very precisely to the sample.

It works similarly to [BMX2WAV](http://childs.squares.net/program/bmx2wav/index.html) that converts
.BME or .BMS files to .WAV file or [OpenSPC](http://membres.multimania.fr/pixels/OpenSPC.html) that converts songs
ripped from games in SNES.


Usage
-----

To convert a Krazy Rain song to a wave file:

- You need to extract the song file first. There is already a tool to do this, so this app won't do it for you.
- Convert all samples to .wav file. Once you unpack the song file, you will get a bunch of XNE and XNT files and
a hell lot of OGG files. There is a tool to convert .OGG files to .WAV files, so this app won't do it for you.
- Use KrazyRenderer to convert .xnt file to .wav file.

### Converting Samples to .wav Files

If you have installed [vorbis-tools](http://wiki.xiph.org/Vorbis-tools), you can use `oggdec` to convert them.
On my Mac and on Linux you could do:

    for I in *.wav; do oggdec "$I"; done

and on Windows you would do

    for %i in (*.wav) do c:\oggdec.exe "%i"

or use a GUI frontend to convert them. But you need to __convert them into 16bit PCM 44,100hz only!__
Or KrazyRenderer won't work.

### Using KrazyRenderer

It is a command line application. All you have to do it pass a .xnt file to it.
Note that __the corresponding .xne file must be present!__ Or KrazyRenderer won't work.

You can also pass a filename as the second argument to have it output to a different file name.

    Usage: krazyrenderer <filename.xnt> [<output.wav>]








