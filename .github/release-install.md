
## Installing

Download the zip for your computer below, unzip it, and copy the `Paragon PR7-M.vst3` folder into your VST3 folder. Close your DAW first if you're replacing an older version.

| OS | VST3 folder |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3` |
| macOS | `~/Library/Audio/Plug-Ins/VST3` |
| Linux | `~/.vst3` |

**macOS:** the plugin isn't code-signed yet, so macOS blocks it until you run this once in Terminal:
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Paragon PR7-M.vst3"`

Then rescan plugins in your DAW.
