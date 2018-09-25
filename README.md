[![Build Status](https://travis-ci.org/Hellmonk/hellcrawl.svg?branch=master)](https://travis-ci.org/Hellmonk/hellcrawl)

# Dungeon Crawl Stone Soup (hellcrawl fork)

#### Hellcrawl is a meme-based fork of DCSS that you can play right now:

* It is a streamlined experience. Content, mechanics, and options from DCSS which are tedious, unpopular, or unnecessary have been removed.
* Notably, this include no food, no curses, no Lair branch, no player ghosts, no item identification, no poison magic, no halflings, no centaurs, and no felids.
* No turning back! In other words, there are no stairs up to previous levels. The only way out is forward, and when you grab the Orb of Zot you can't just walk it back through the dungeon, you must carry it through **HELL**!
* You can play it online. Several official DCSS servers also host Hellcrawl! http://crawl.berotato.org:8080/#play-dcss-hellcrawl-cbro is the link for a USA-based server, and https://crawl.project357.org/ is a server in Australia. It is also available on the Japanese server (lazy-life.ddo.jp) and the Korean server (webzook.net). 
* Downloads available at https://github.com/Hellmonk/hellcrawl/releases
* Information and discussion about Hellcrawl is primarily found on the Tavern discussion thread. Go to the following link in order to find the best, fullest changelog or to share your thoughts:
* https://crawl.develz.org/tavern/viewtopic.php?f=17&t=21527
* You may also occasionally spot the project maintainer, hellmonk, in the dcss and dcss-variants channels of the roguelikes discord.

#### The readme from DCSS 0.19 may also be of use to you, but it may also be dead wrong. It is preserved in its entirety below.

Dungeon Crawl Stone Soup is a game of dungeon exploration, combat and magic, involving characters of diverse skills, worshipping deities of great power and caprice. To win, you'll need to be a master of tactics and strategy, and prevail against overwhelming odds.

## Contents

1. [How to Play](#how-to-play)
3. [Community](#community)
5. [How you can help](#how-you-can-help)
4. [License and history information](#license-and-history-information)

## How to Play

If you'd like to dive in immediately, we suggest one of:

* Start a game and pick a tutorial (select tutorial in the game menu),
* Read [quickstart.txt](crawl-ref/docs/quickstart.txt) (in the [docs/](crawl-ref/docs/) directory), or
* For the studious, [read Crawl's full manual](crawl-ref/docs/crawl_manual.rst).

There is also an ingame list of frequently asked questions which you can access by typing
`?Q`.

#### Internet Play

You can play Crawl online, competing with other players or watching them. Click "Play Online Now!" on [the Crawl homepage](https://crawl.develz.org/) to find your closest server. You can play in your browser or over SSH.

#### Offline Play

Both classical ASCII and tiles (GUI) versions of Crawl are available to [download for Linux, Windows and OS X](https://crawl.develz.org/download.htm).

## Community

### Web
Our official homepage is [https://crawl.develz.org](https://crawl.develz.org/). You can find online play, offline downloads, a community forum and the development bug tracker/wiki.

### IRC
Chat with fellow crawlers in [##crawl on irc.freenode.net](https://webchat.freenode.net/?channels=##crawl), or talk development in [##crawl-dev](https://webchat.freenode.net/?channels=##crawl-dev).

## How you can help

If you like the game and you want to help make it better, there are a number
of ways to do so:

### Reporting bugs

At any time, there will be bugs -- finding and reporting them is a great help.
Many of the online servers host the regularly updated development version. Bugs
should be reported to [our bug tracker](https://crawl.develz.org/mantis/). Besides pointing out bugs, new ideas on how to improve interface or gameplay are welcome. These can be added to [the development wiki](https://crawl.develz.org/wiki/).

### Map making
Crawl creates levels by combining many hand-made (but often randomised) maps, known as *vaults*. Making them is fun and
easy. It's best to start with simple entry vaults: see [simple.des](crawl-ref/source/dat/des/arrival/simple.des) for examples. You can also read [the level-design manual](crawl-ref/docs/develop/levels/introduction.txt) for more help.

If you're ambitious, you can create new vaults for anywhere in the game. If you've
made some vaults, you can test them on your own system (no compiling needed) and
submit them to [our bug tracker](https://crawl.develz.org/mantis/).

### Monster Speech & Item Descriptions
Monster speech provides a lot of flavour. Just like vaults, varied speech depends
upon a large set of entries. Speech syntax is effective but unusual, so you may want to read [the formatting guide](crawl-ref/docs/develop/monster_speech.txt).

Current item descriptions can be read in-game with `?/` or out-of-game
them in [dat/descript/](crawl-ref/source/dat/descript/). The following conventions should be more or less obeyed:
* Descriptions ought to contain flavour text, ideally pointing out major weaknesses/strengths.
* No numbers, please.
* Citations are okay, but try to stay away from the most generic ones.

### Tiles
We're always open to improvements to existing tiles or variants of often-used tiles (eg floor tiles). If you want to give this a shot, please [contact us](#community) via forums or IRC. In case you drew some tiles of your own, you can submit them to [our bug tracker](https://crawl.develz.org/mantis/).

### Patches
For developers (both existing & aspiring!), you can download/fork the source code and write patches. Bug fixes as well as new features are very much welcome.

For large changes, it's always a good idea to [talk with the dev team](#community) first, to see if any plans already exist and if your suggestion is likely to be accepted.

Please be sure to read [docs/develop/coding_conventions.txt](crawl-ref/docs/develop/coding_conventions.txt) too.

## License and history information

Crawl is licensed as GPLv2+. See [licence.txt](crawl-ref/licence.txt) for the full text.

Crawl is a descendant of Linley's Dungeon Crawl. Development of the main branch stalled at version 4.0.0b26, with a final alpha of 4.1 being released by Brent Ross in 2005. Since 2006, the Dungeon Crawl Stone Soup team has continued development. [CREDITS.txt](crawl-ref/CREDITS.txt) contains a full list of contributors.

Crawl gladly uses the following open source packages; thanks to their developers:

* The Lua scripting language, for in-game functionality and user macros ([license](crawl-ref/docs/license/lualicense.txt)).
* The PCRE library, for regular expressions ([license](crawl-ref/docs/license/pcre_license.txt)).
* The SQLite library, as a database engine ([license](https://www.sqlite.org/copyright.html)).
* The SDL and SDL_image libraries, for tiles display ([license](crawl-ref/docs/license/lgpl.txt)).
* The libpng library, for tiles image loading ([license](crawl-ref/docs/license/libpng-LICENSE.txt)).

Thank you, and have fun crawling!
