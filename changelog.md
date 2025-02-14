# DecimalPercentages Changelog
## 1.1.3
- *Hopefully* fix a bug where the decimal percentages for some level cells displayed some variation of `0.000%`.
- Refactor a few things here and there.
## 1.1.2
- Port to 2.2074.
## 1.1.1
- Added support for saving percentages for gauntlet variants of a level ID.
## 1.1.0
- Refactor how percentages are saved.
- Fix decimal points not being applied to best % scores from practice mode in the % progress bar label.
- Fix one last odd edge case where percentages displayed in the progress bar weren't equal to what was recorded by the mod.
- Fix a bug where percentages saved from Daily/Weekly/Event variants of a level ID were applied to regular variants of the same level ID, despite different amounts of progress.
- As a consequence of both of these bug fixes, DecimalPercentages is not as aggressive with applying its decimal points, more significantly so to levels with percentage progress before installing the mod. (Hopefully.)
## 1.0.1
- Quick compatibility bugfix when DeathScreenTweaks isn't enabled/installed.
- Quick bugfix for "New Best" screens when dying on rated levels.
## 1.0.0
- Initial release (on GitHub).