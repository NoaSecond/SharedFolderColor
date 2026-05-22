# SharedFolderColor

Unreal Engine plugin for sharing folder colors across the team via the repository.

## Overview

SharedFolderColor helps artists and developers quickly identify folders by assigning colors, and share those choices with the team by saving them to a versioned JSON file.

## Features

- Automatically save color changes to a JSON file that can be committed to the repository

## Requirements

- An Unreal Engine project
- A compatible Unreal Engine version for this plugin
- Access to the project’s `Plugins` directory

## Supported Platforms

- Windows (Win64)
- macOS
- Linux

## Installation

1. Copy the `SharedFolderColor` plugin folder into your project’s `Plugins` directory.
	- Example: `YourProject/Plugins/SharedFolderColor`
2. Open the project in Unreal Engine.
3. If prompted, enable the plugin and rebuild the project.
4. Restart the editor if Unreal requests it.

## Setup

1. Open your project in Unreal Engine.
2. Navigate to **Edit > Plugins**.
3. Search for **SharedFolderColor**.
4. Enable the plugin.
5. Restart Unreal Engine if required.

## Usage

After installation, use the context menu in the Content Browser to apply a color to any folder:

1. Save any open work.
2. Right-click a folder in the Content Browser.
3. Choose a color.
4. The plugin will automatically detect color changes and save them to a JSON file so they can be committed and pushed to the repository.

## Synchronization

- Colors are stored in a local JSON file (path is configurable) which can be added to your Git repository.
- To share folder colors with the team, commit and push this JSON file from your project.
- When a team member updates the JSON from the repository, colors are applied to the project on load or after a reload.

## Troubleshooting

### Plugin does not appear in the Plugins window

- Verify the plugin is located in the correct `Plugins` folder.
- Verify the plugin archive has been fully extracted (unzipped).
- Confirm the `.uplugin` file exists.
- Remove the `Binaries` and `Intermediate` folders from the plugin directory.
- Regenerate project files and reopen the project.


### Unreal asks to rebuild

- Make sure the plugin version matches your Unreal Engine version.
- Rebuild the project from your IDE or the Unreal Editor.

### Colors do not persist or display

- Confirm the plugin is enabled.
- Check the Unreal output log for errors.
- Make sure you have write permissions to the project folders where plugin settings are stored.

## License

This plugin is distributed under the Fab Adapted License. See the `LICENSE` file in this repository for the full text.

## Support

For issues, feature requests or support, please open an issue on the project repository or contact the author:

- Repository: https://github.com/NoaSecond/SharedFolderColor
- Author: Noa Second — https://noasecond.com