![omiPlayer-banner.png](https://bitbucket.org/repo/XKdKdL/images/2129969955-omiPlayer-banner.png)

# omiPlayer #

Media player for Oculus Rift. It can playback mp4 (h.264 with yuv420p) and png/jpg. The **Auto-reload** feature let you save some clicks when rendering :)

## Installation ##
Download the latest version [https://bitbucket.org/omigamedev/omiplayer/downloads/omiPlayer.zip](https://bitbucket.org/omigamedev/omiplayer/downloads/omiPlayer.zip)

**OpenAL**: if you don't have it installed you need to install. It's a common audio library usually installed by default in OSX and Linux.
http://www.openal.org/creative-installers/oalinst.zip

**vcredist**: you may need Microsoft Visual Studio 2013 runtime libraries for this to work.
https://www.microsoft.com/en-us/download/details.aspx?id=40784

The installation is a simple windows batch script "**install.bat**". Right click on it and "**Run as administrator**" because it copies the contento into "C:\Program Files\omiPlayer" which is not writable otherwise and the installation will fail. To uninstall the player just run **uninstall.bat** in the same way.
It also register a Shell Integration which means you can right click on the file you want to play and chose Open with omiPlayer.

![open-with-omiplayer.png](https://bitbucket.org/repo/XKdKdL/images/4285692939-open-with-omiplayer.png)

## Features ##
This player is designed to be highly multithreaded and it's able to reproduce 4K videos at 60fps smoothly while keeping the rendering at a constant 75Hz update rate. It uses LibOVR 0.4.4 and will be soon updated to the 0.6.0 when it will be released.

* **Shell integration**: just right click on a video or image and open with omiPlayer

* **DirectMode**: for the best experience the player supports the Oculus DirectMode that offers the best performace.

* **Mirror**: a window mirrors what it's in the Oculus so others can see what we are looking at.

* **Auto-reload**: this feature works only with images and it's awesome for the artists that can render a frame and instantly get it into the HMD.

## Video ##

Video demonstration: [https://www.youtube.com/watch?v=jHW5Xewj65A](https://www.youtube.com/watch?v=jHW5Xewj65A)

![omiplayer-yt.PNG](https://bitbucket.org/repo/XKdKdL/images/345359471-omiplayer-yt.PNG)

## Screenshots ##

![player.PNG](https://bitbucket.org/repo/XKdKdL/images/2765498358-player.PNG)