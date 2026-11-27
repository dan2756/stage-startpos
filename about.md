# Stage Startpos

Adds the percent your current stage run has to reach to the percentage at the top of the screen, like `44.27%/74.27%`.

Needs Zhulis's [stage practice mod](mod:zhulis.blitzkrieg), which Geode installs with it.

The target is the end of the run that starts at your start position, in the stage you're currently on. It is read again after each reset and start-position switch finishes.

In levels with a stage profile the percentage is centred under the progress bar, and the bar gets a line where the run starts and another where it has to get to.

The moment the run reaches its target, a little chime plays and the percentage turns green, including when [Golden Best](mod:firee.goldenbest) is active. Green stays until the next attempt, when the normal percentage styling returns.

The target stays for the whole attempt. Once the practice mod marks the run done, which happens when the attempt ends, that start position shows just the percentage until the stage moves on.

The percentage also stays as it is when the level has no stage profile linked, or when the attempt doesn't start on one of the stage's runs.
