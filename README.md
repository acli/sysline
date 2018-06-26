This is Ambrose's [modded][1] version of sysline,
based on Bauke Jan Douma's Linux port.
Bauke Jan's original README file is in [README][2].

I don't remember when I did the modding,
but it was a long time ago.
I've decided to put this here
only because I kept forgetting
where I've put my modded version.

You probably already have a clock on your desktop's panel,
so how is sysline still relevant?
Because you can make it beep every half hour,
and no desktop clock (that I know of) does that.
This is important if you want to follow
the 30/30/30 rule recommended by some optometrists:
you don't need to constantly look at the clock
to know it's time to look 30 feet away..

For the beep to work you'll need to
[configure PulseAudio to emit beeps][3].
You also need to make sysline run in your xterm;
`TERM=xterm+sl sysline` often works.

[1]: CHANGES.unofficial
[2]: README
[1]: https://www.rohanjain.in/bell/

