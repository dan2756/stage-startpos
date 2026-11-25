# Blitzkrieg Target

Adds the percent your current [Blitzkrieg](mod:zhulis.blitzkrieg) run has to reach to the percentage at the top of the screen, like `44.27%/74.27%`.

The target is the end of the run that starts at your start position, in the stage Blitzkrieg is currently on. It is read again after each reset and start-position switch finishes.

In levels with a Blitzkrieg profile the percentage is centred under the progress bar, and the bar gets a line where the run starts and another where it has to get to.

The moment the run reaches its target, a little chime plays and the percentage turns green, including when [Golden Best](mod:firee.goldenbest) is active. Green stays until the next attempt, when the normal percentage styling returns.

The percentage stays as it is when the level has no Blitzkrieg profile linked, or when the attempt doesn't start on one of the stage's runs.
