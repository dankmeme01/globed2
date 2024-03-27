## v1.3.7

* Fix being unable to restart a level with the Confirm Restart feature in BetterPause (and other bugs similar to this)

## v1.3.6

* Fix crash on startup on Android

## v1.3.5

* Added Badges (Userlist, Roomlist, In Game, Voice Overlay)
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
