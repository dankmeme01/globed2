## v1.6.0

* Add a way to <cg>pin a specific level</c> in a room
* \-- If you are a room creator, there will now be a <cp>new button</c> whenever opening an online level
* \-- The chosen level will be <cg>pinned</c> at the top of the player list in the globed menu
* \-- Thanks [TechStudent10](https://github.com/TechStudent10/) for implementing this!
* Add a <cg>popup</c> showing the <cy>changelog</c> of the most recent update
* Add proper user visibility settings (TODO ACTUALLY MAKE UI!)
* \-- You can now choose to separately hide yourself from the player list, the level, or the invite list.
* Show all the <cp>badges</c> of a player instead of just one at a time (for example <cg>Mod</c> & <cp>Booster</c>) (thanks Kiba!)
* When playing in a <cj>Death Link</c> room, the faster reset option is now <cy>forcefully disabled</c> for all players (to prevent <cr>desync</c>)
* Automatically <cr>block</c> invites from users that you have <cr>blocked</c> on GD
* Add a Force Progress Bar setting, that makes it so that the progress bar is enabled if there's other players on your level, when it's otherwise disabled (thanks TechStudent10!)
* Add Node IDs dependency
* Remove Custom Keybinds dependency from Android, as it was unused
* Various security improvements

Bugfixes:

* Fixed some <cy>login issues</c> (login taking a very long time and sometimes failing for no reason)
* Fixed player names and status icons not rotating together with the camera (i.e. arrow trigger)
* \-- You can return to the old behavior by disabling the "Rotate names" setting
* \-- Thanks [TheSillyDoggo](https://github.com/TheSillyDoggo/) for implementing this!
* Fixed crash caused by closing the globed menu too fast
* Fixed crash when muting a user that has exited the level
* Fixed room name <cr>overlapping</c> other UI elements in the room listing
* Fixed player count going up to 4 digits on android (and cap to 10000 instead of 65535)
* Fixed the room listing getting <cr>closed</c> when trying to join a room, even if joining fails
* Fixed <cg>"Player"</c> with default icons showing up in the player list
* Change max web request timeout to be <cg>10 seconds</c> in most places instead of <cy>3 or 5 seconds</c>

## v1.5.1

* Fix Globed not loading on Android due to missing Custom Keybinds dependency

## v1.5.0

* Add a Death Link setting to rooms, which causes all players in the level to die if one of them dies
* \-- **Huge thanks to <cg>Alphalaneous</c> for making the DeathLink mod and allowing us to use their idea**
* Add ability for the room owner to close the room (kicks all players)
* Change the Receive Invites From setting to friends only by default instead of everyone
* Fix the player not being disconnected from the server if they log out of their GD account
* Fix crash when closing the featured level list too fast
* Fix spammy error popups on self-hosted servers when no featured level is selected
* Add an age warning to the discord invite button

## v1.4.5

* Add player counts to the featured level list
* Make player names in invites clickable to bring up their profile
* Fix some bugs with the featured level list
* Fix level ordering in the saved tab (again)
* Fix "Receive Invites From" setting crashing the game
* Fix the next arrow button showing on the last page of the featured level list

## v1.4.4

* Add Globed Featured Levels (thanks [Kiba](user:4569963))
* \-- A level will be chosen every few days to be featured on Globed
* \-- Levels can be rated Featured, Epic and Outstanding
* Levels played on Globed are now put at the top of the saved levels list, rather than the bottom
* Hopefully fix room list not loading on some devices
* Change the icon used in Discord RPC integration to a better one
* Various small fixes and UI changes

## v1.4.3

* Fix crash on ARM MacOS when a player with an animated name is online
* Fix rare crash with devtools
* Add error message when entering wrong room password
* Minor UI changes

## v1.4.2

* Put yourself at the top of the player list (thanks [Kiba](user:4569963))
* Add integration with the Discord Rich Presence mod
* \-- If you have the mod installed, you will have a special rich presence while playing on Globed
* Playing an unlisted level in the editor will no longer show the level on the level list
* \-- Note: joining an unlisted level NOT through the editor will still cause the level to be shown on the level list, as the game does not know whether the level is unlisted or not.
* Add option to disable multiplayer in the level editor
* Fix crash when completing created levels on ARM MacOS
* Fix server list being broken on MacOS
* Fix joining a room while in a level causing visual glitches until you reopen the level
* Small UI changes and fixes
* Make the main room menu significantly more optimized
* Fix weirdness when opening someone's levels or lists while in a level and then coming back
* Fix hitboxes of mini icons being too big in collision rooms
* Remove version checks when adding a new server
* Small fixes to asset preloading

## v1.4.1

* Add MacOS support
* Improve the room listing (now shows player count/limit and whether the room has collision enabled or a password set)
* Sort room list by player count
* Fix room list not starting at the top
* Fix the Audio Device and Receive Invites From settings not saving when exiting the game
* Fix weirdness when receiving an invite while in a level

## v1.4.0

* Overhaul the room system
* \-- You can now send room invites to other players
* \-- Rooms have various settings, i.e. password, collision, player limit, name, invite-only
* \-- Public room listing that lets you discover rooms without having to be invited
* \-- You can control whether you can be invited by anybody, only friends, or by nobody
* Add user roles
* \-- Staff, supporters, etc. will have special name color, text chat color, and a badge near their name
* \-- Thanks to [LimeGradient](user:7214334), [Terma](user:4706010) and [Capeling](user:18226543) for help!
* Add editor compatibility
* \-- This does **NOT** mean you can build levels together!
* \-- If you are the creator of a level, you will be able to see other people playing while in the editor
* \-- When [EditorCollab by alk1m123](https://github.com/altalk23/EditorCollab) comes out, you will also be able to playtest non-uploaded levels together with others
* \-- Experimental, might have issues
* Add automatic reconnection to the server in case of a disconnect
* Add credits to the globed menu (thanks [TechStudent11](user:20284359) for partial help)
* Fix all unsynced clock issues (auth no longer uses TOTP)
* Fix most "unhandled packet" errors by rewriting networking core
* Fix many connection related bugs
* Ask the user if they want voice chat enabled or not on first startup
* Add a Compressed Player Count setting which changes the "X players" label to be more compact (thanks [Uproxide](user:25397826)!)
* Add support & voice chat guide buttons in the menu (thanks [Kiba](user:4569963) for the design)
* Add a gradient and an icon to your friends on the player list
* Add a way to hide yourself from the player list in menus (thanks [TechStudent11](user:20284359))
* Redesign the main menus and change textures for many of the icons
* Add much better error messages for outdated clients
* Don't play effects and animations if the player is too far, to decrease lag
* Replace the Overlay settings tab with Menus and move overlay settings into Level UI
* Pin the main server to the top of the server list and disallow deleting or modifying it
* Make some of the UI look better on weird aspect ratios (such as 4:3)
* Fix "already inside of a level" error sometimes appearing when it shouldn't, and add a way to bypass it
* Fix settings menu looking a bit broken due to a Geode update
* Fix the placeholder green icon color being a slightly different shade than the actual default green
* Fix a missing texture crash when asset preloading is disabled
* Fix progress indicator line going out of the progressbar (thanks [Terma](user:4706010))
* Lots of internal restructuring and rewrites, for improved performance and user experience
* Protocol optimization (slightly less bandwidth usage)
* Change the compiler used for Windows releases

Some of the features have been co-developed by [ninXout](user:7479054), who has now also become an official Globed developer :)

## v1.3.7

* Add a mysterious button to the room menu
* Fix interpolation issues related to vsync
* Fix being unable to restart a level with the Confirm Restart feature in BetterPause (and other bugs similar to this)

## v1.3.6

* Fix crash on startup on Android

## v1.3.5

* Fix crashes and other weird behavior when opening profiles of others when in a level
* ^ also makes it impossible to open another level while already in a level
* Fix issues with zip file texture packs
* Fix own name setting not working on Mac
* Fix crash when having the confirm quit menu open without a visible pause menu and trying to quit the level (yes this is very specific and there's like 1 person who ever encountered this)
* Fix rare crash when exiting a level
* Fix rare crash when exiting the game

## v1.3.4

* Fix player collision being broken in the editor
* Fix crash when opening player list
* Fix crash when quickly closing the level browser on Android
* Make the icons above players change their opacity too when changing player opacity

## v1.3.3

* Speed up asset preloading even more (death effects now don't preload if they are disabled or if Default death effects is enabled)
* Add a way to hide specific players
* Add an option to load assets when first joining an online level instead of on the loading screen (for Colon)
* Add an option that shows your own name above your icon
* Add ability to copy room IDs by pressing the title
* Death effects and spider animations of other players now play while paused
* Some player animations are now paused when that player is also paused
* Fix some (but not all) crashes related to asset preloading and texture packs
* Fix platformer squish animation looking wrong when jumping sideways from a wall
* Fix (yet again) progressing in levels with start positions

## v1.3.2

* **Improve loading times** - Asset preloading is now up to **3-5 times faster** on Windows (not as fast on other platforms, but still slightly faster)
* Fix rare crash when exiting a level
* Fix some crashes in the level browser on android
* Improve the platformer squish animation slightly
* Make the "outdated client" message more straightforward
* Add player counts to tower levels (**requires** Node IDs mod to be installed first to work)
* Fix progressbar icons having wrong glow when the Colored Icon Kit mod is installed
* Try to fix progressing in levels with startpos (again)
* Try to fix crashes when using a Level Edit hack
* Hide loopback devices from audio settings
* Add a way to manually set packet limit
* Fix players not appearing in a level until you move away from the spawn position

## v1.3.1

* Fix being unable to join a room on some Android devices (due to room ID input being limited to 5 characters)
* Fix the "application was in the background for too long" disconnect error sometimes appearing when it shouldn't
* Fix portal gauntlet progress resetting
* Fix some voice chat crashes on Android
* Fix progressing in levels with start positions
* Fix undeafen notification not showing up in platformers if you had proximity chat on
* Remove voice chat hints on Mac and Android (since you can't really press V there)
* Disable audio device setting on Mac and Android
* Added a report button in the user list that redirects to the Globed Discord
* Server level lists now show up to 30 levels by default, with a new option being added to increase it back to 100

## v1.3.0

**Add support for Android (both 32-bit and 64-bit) and MacOS!**

This is a **required** update, and you will not be able to connect to the server until you update the mod.

Please note that those platforms are not as thoroughly tested, so they may have issues. Additionally, talking in voice chat is **currently impossible** on all platforms except Windows, but you will still be able to hear others.

Other changes include:

* Redesign the settings menu to be more convenient (it now has tabs!)
* Enable Lower audio latency by default
* Enable Hide conditionally by default (for ping overlay)
* Add the squish animation for other players whenever they jump in platformer mode
* Add a setting to always show players, even if they are invisible
* Add deafen notifications (can be disabled in settings)
* Add better server messages for failed login attempts
* Add a voice volume slider in the player list on the pause menu
* Add player name colors to the room list
* Slightly improve bandwidth usage by optimizing the protocol
* Made the jump to level button jump to a main level page if it's a classic main level
* Fix the ball icon sometimes being displayed as a cube with Default mini icons enabled
* Fix crashes due to the menu button being removed by another mod
* Fix player count on main levels not animating together with the button
* Fix player count being in a wrong place with Compact Lists installed
* Change the scaling and pop animation of many buttons to look a bit more pretty

Thank you [Capeling](user:18226543), [ninXout](user:7479054) and [TheSillyDoggo](user:16778880) for their first contributions to the mod in this update :)

## v1.2.5

* Fix crash when browsing levels
* Fix player count label being shown in lists (in a silly place)
* Fix audio test in settings being broken
* Hide player count on levels with no people

## v1.2.4

* Add name colors to player list
* Fix the player count label being displayed when not connected to a server
* Fix player count being displayed in wrong places (like in map pack list)
* Fix the game not closing properly sometimes

## v1.2.3

* Add an option for proximity voice in non-platformer mode
* Add a player count to main levels
* Add player counts to the level browser
* Make it harder to accidentally close an error/notice popup
* Fix dual icon still being visible after exiting dual mode
* Slightly fix the layout of the voice chat overlay cells
* Fix the robot fire staying in the air after death
* Fix name searching in the admin panel

## v1.2.2

* Add a voice overlay to show who's currently speaking
* Don't record audio if voice chat is disabled (oops)
* Try to fix crashes that don't even show an error popup
* Fix a crash when connecting to an invalid server
* Fix login issues when the username has spaces
* Add a setting that lets you change opacity of the icons under the progressbar (or edge of the screen in platformer)
* Scale down the name of a server if it's too long

## v1.2.1

* Refresh server list more often (fixes some crashes and connection issues)
* Make the player list in-game more compact
* Fix some issues with the admin panel

## v1.2.0

* Fix deadlock on packet limit test failure
* Fix audio stack overflow due to high deltatime
* Add a geode setting to disable asset preloading (in case the game doesn't launch)
* Make own icon being above everyone else on the progress bar
* Server owners now have an advanced admin panel to moderate the server

## v1.1.2

* Add player searching in the player list menu
* Fix lag when activating voice chat
* Fix rare audio crashes caused by data races
* Fix crash when changing the Position setting
* Fix crash when editing a central server
* Fix robot fire sometimes randomly appearing in a level
* Fix login failed issue when changing the url of a server

## v1.1.1

* Add pages to the online level list (and fix some levels not appearing)
* Add a Hide Nearby Players setting that allows you to make nearby players transparent
* Fix some more network related issues (specifically keeping the TCP connection alive)
* Fix huge lag related to packet limit & fragmentation
* Fix huge lag when exiting a level
* Fix some audio related issues
* Fix the mute button not working immediately in platformer
* Fix the cube icon being default in levels starting with a non-cube gamemode
* Allow editing the server name even if the server is offline
* Fix being able to quit/resume the level while having the user popup opened on the pause menu
* Fix some lagspikes when recording audio
* Fix crash when connecting to an invalid server

## v1.1.0

Globed now uses a different networking protocol (hybrid TCP/UDP instead of just UDP). This should fix **many (but not all!) of the connection related issues**, such as players not loading in a level, or the player list popup taking an infinite amount of time to load.

That means this is a **required** update which bumps the protocol version to 2. You will not be able to connect to an updated server without updating the mod to this version.

If you are a server owner, please read the [server changelog](https://github.com/dankmeme01/globed2/blob/main/server/changelog.md) as there have been some changes to the server.

Additional changes include:

* Improve the **account verification** system (and enable it on the main server)
* Fix the freezes/crashes on the loading screen and update the message text at the bottom when loading icons
* Add a connection calibration button in settings (try to use it if you have connection issues)
* (Maybe) fix crash when applying a texture pack with Texture Loader
* Fix audio crash
* Add a volume indicator in the pause menu player list
* Fix the TPS cap setting being impossible to change
* Fix bug with blank names when clicking on a user to open their profile
* Make the mute button work immediately instead of with a delay
* Add an automatic rollback system for a certain save bug related to gauntlets
*  ^ in versions before v1.0.5 there used to be a bug that would cause your savefile to have some invalid levels. Now those will be automatically removed on startup.

## v1.0.5

* Preload icons on the loading screen (longer loading time but hopefully a lot less lagspikes in-game now)
* Fix disconnecting when minimizing the game
* Room popup now doesn't auto refresh, and sorts players alphabetically (also your friends are now shown before everyone else!)
* ^ same applies for the in-game player list popup
* Scroll position no longer resets when refreshing a list
* Changed the voice chat volume slider to be ranged from 0% to 200% instead of 100%
* You can now hear yourself when testing audio in settings
* Make asset preloading optional (if you have weird texture bugs, disable it in settings!)
* Hopefully fix a crash when disconnecting the active audio input device
* Maybe fixed death effects crashing the game
* Fix some crashes caused by corrupted data
* Fix gauntlet levels sometimes being broken
* Fix player opacity not working properly on spider and robot
* Add a message indicating how to use voice chat

## v1.0.4

* Make incompatible with More Level Tags (the issues were too hard to fix sorry)
* Make deafening immediate (you will stop hearing others as soon as you press B, no need to wait)
* Fix crashes on old CPUs, now for real

## v1.0.2

* Use github actions for releases (hopefully no more crashes on old CPUs)
* Add a volume slider for voice chat
* Add a button that opens the discord server
* Fixed a crash in the editor
* Fixed an incompatibility with the mod More Level Tags, should no longer crash when opening a level
