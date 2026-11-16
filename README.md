# Digital Sobriety Chip for Flipper Zero

A Flipper Zero app that works as a digital sobriety chip. It counts the days since your sober date, using the Flipper's real-time clock.

## Features

- **Day counter on a chip**: the number of days sober, shown big inside a ring.
- **Calendar breakdown**: years, months and days since your sober date, e.g. `3y 8m 6d`.
- **Milestone chips**: the last chip you earned (●) and the next one (○), with a countdown in days.
  Milestones: 24 hours, 30 / 60 / 90 days, 6 / 9 months, 1 year, 18 months, then every year.
- **Saved date**: the date is kept on the SD card (`/ext/apps_data/sobriety_chip/sobriety.conf`), so it survives restarts.
- The count updates by itself at midnight.

## Controls

| Screen      | Button        | Action                         |
|-------------|---------------|--------------------------------|
| Chip        | OK            | Edit sober date                |
| Chip        | Back          | Exit                           |
| Date editor | Left / Right  | Select year / month / day      |
| Date editor | Up / Down     | Change value (hold to repeat)  |
| Date editor | OK            | Save                           |
| Date editor | Back          | Cancel (exit on first launch)  |

On first launch the date editor opens with today's date already filled in. You can't pick a date in the future.

> The count depends on the Flipper's clock. If it's wrong, set it under **Settings → System → Date/Time**
> or sync it with qFlipper.

## Building

You need [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
pip install --upgrade ufbt
ufbt            # builds dist/sobriety_chip.fap
ufbt launch     # builds, installs and runs it on a connected Flipper
```

Or download the `.fap` file from the latest **Build** workflow run on GitHub Actions. Copy it to
`/ext/apps/Tools/` on the SD card.

## Tests

The date math (leap years, month lengths, milestones) is plain C and is tested on the host:

```bash
make -C tests
```
