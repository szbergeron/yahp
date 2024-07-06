#Because getting an AvantGrand would just be way too easy

Inspired by [Greg Zweigle's DIY Hybrid Piano](https://github.com/gzweigle/DIY-Grand-Digital-Piano) and the [Cybrid Project](https://github.com/ekumanov/cybrid).

I don't come from a hardware/EE background, so a lot of that stuff was done by feel rather than through meticulous science. Instead, I tried to all of the "magic" through software. The sensors are probably not "ideal", but they are cheap and dumb simple. I didn't use trimmers or anything like that to calibrate, opting instead to gather raw measurements and then apply software correction. I also opted to use onboard ADCs instead of external ones or any kind of extra voltage reference. This absolutely introduces some inaccuracy in the lower bits of the raw measurements, but again--software magic :)

The flip side of using such a simple design from an EE perspective is that fabrication is _dumb simple_, even if technically more expensive when looking at the BOM. Each key uses a single sensor, and there is an MCU for every 16 keys, plus some basic power filtering all on a PCB. I'm spectacularly bad at soldering, so this has as little of it as I could get away with. This project is aimed at being an easy "kit" to get components for and assemble. Follow along if you'd like!
