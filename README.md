# Dungeon Crawl Stone Soup: Circus Animals variation

---

NOTICE: The [changelog-ca](https://github.com/jeremygurr/dcssca/blob/master/README.md) file contains the latest detailed changes. We'll try to keep this file as a high level summary of the fork. If you want specifics, check out the changelog!_

---

There are two known servers where you can play Circus Animals!

* http://crawl.homedns.org/ (arizona, USA)
* https://crawl.project357.org/ (sydney, AUS)

Checkout branch v1.5.2 if you want to try it on your desktop. The master branch is where future, save game breaking features get implemented, and then broken off.

To do so from a git repo, install git on your system, then type:

```git clone https://github.com/jeremygurr/dcssca.git -b v1.5.2```

This fork of DCSS is a playground for some ideas I wanted to try. Many changes center around increasing the richness of the species in DCSS. I take a species that I consider boring to play, then alter that species until it feels like a compelling choice. I am not as experienced as many others are, and likely some of these changes may break important things that I don't yet understand because of that . But because I am having so much fun with this fork, and it doesn't take much effort to share it with others, here it is. Maybe some of my crazy ideas can be merged into the main DCSS repo. You are most welcome to submit changes to this fork, or even make another fork if you don't like the direction of this one. This fork was in sync with the original DCSS repo as of Apr 2016. I intend to keep merging in non-conflicting changes from the [main DCSS project](https://github.com/crawl/crawl).

Check out [FUTURE.md](https://github.com/jeremygurr/dcssca/blob/master/FUTURE.md) for details about further features I'm considering.

Feel free to create "issues" here on github for any problems with the crawl.homedns.org server or bugs in the game itself. Ideas you have that you think are in line with my goals are also welcome, or concerns you have about future plans.

A player made an IRC channel for discussion of this fork. Grab an IRC client and come join the conversation! The channel is [##circusfork on the Freenode IRC server](https://webchat.freenode.net/?channels=##circusfork). If you're already hanging out in ##crawl, all you have to do is type /join ##circusfork!
Objectives

(My goals aren't necessarily that different from mainstream crawl, just different in exactly how those goals are interpreted and implemented...)



Major modifications from the original DCSS
v1.4.2

Summoning changes
   * Summoning simply reduces your mana until the summons are gone, but have no duration.
   * Summons multiply the spell's base cost.
   * Added a "release summons" ability, so you can free up your mana if you no longer need those summons.
   * If you release a summon, all of the mana used to summon it is returned.
   * The number of summons a player can have is only limited by max mana, with a cap of 20 total summons.
   * When a summon dies, they player gets 1/2 of the mp used for that creature
   * When going up or down stairs, if there are no monsters in sight, summons automatically follow you.



Pan has been shortened
   * All 4 rune floors are guanteed within the first 25 floors.

v1.4.1

Transmutation
   * transformations don't time out
   * power of the transformation is based on spell power
   * when the player takes any offensive action (attacks, casts, evokes), the spell failure of the form is tested. Once the spell "fails" 5 times, the form unravells and the player returns to normal.
   * each failure chance is cut in half towards the edges, so 60% failure becomes 80% failure, 20% failure becomes 10% failure. so if you can't get the failure below about 30%, you won't be able to maintain the form for long.
   * undead can transmute
       * in living forms, undead can also mutate

v1.3

Wand recharging
   * Wands gain half of the normal capacity for that wand type. So a healing wand gives 5 each recharge.
   * Wands lose 1/2 of what they gain from their capacity
   * But they will never end up with less than 2 charges

Hill Orcs
   * Gave them Summoning +3, because we needed another good summoner.
   * Gave them Bad Dna 1, because they were too awesome overall, and need a nerf.

Doors can be closed even if there are items on the floor

All races can wear cloaks. (including Fe and Op)

Mummy curses a little more threatening
   * 50% chance that they will curse weapon or ring slots
   * otherwise they just pick a random slot to curse
   * 1/10 chance of triple cursing

Mutations
   * added a bunch of super rare mutations that could really throw a curveball at the player.
   * new "dna" mutations, which are hard to get, and hard to lose.
       * good dna: increases chances that mutators will add a good mutation. (a "mutator" is any source of mutation)
       * bad dna: increases chances that mutators will add a bad mutation
       * clean dna: increases chances that mutators will remove a mutation when heavily mutated
       * resilient dna: lowers chances of a mutator causing the player to lose a good mutation
       * weak dna: increases chaces of a mutator causing the player to lose a good mutation
       * short dna: lowers the player's total mutation capacity, meaning a mutator will be likely to remove mutations to keep the total low.
       * long dna: increases the player's total mutation capacity.
       * focussed dna: increases the chance that a new mutation will be an enhancement to an existing one
       * unfocussed dna: reduces the chance that a new mutation will be an enhancement to an existing one

Sif Muna
   * get book gifts at 2* piety instead of 5*
   * channeling ability has been completely rewritten. For 50 piety, all spells cost 0 mp for 20-40 turns, based on invocations.
   * passive ability: conserve MP: with higher invocations levels, spells cost less MP to cast. Starts at 1*.

Added stamina points, alongside magic points and health points
   * four exertion levels
       * normal mode (press 'c' if in another mode)
           * no stamina cost for most actions, except for spell casting which has a stamina cost based on spell hunger
           * movement speed is 1.1 for all species, except naga, which is 1.4
           * damage, spellpower, accuracy, stealth, evasion modifications from vanilla crawl:
               * standard difficulty: 100% of vanilla crawl (means will do the same damage in normal mode as in vanilla)
               * challenge difficulty: 75% of vanilla crawl
               * nightmare difficulty: 50% of vanilla crawl
       * power mode (press 'e' if in another mode)
           * stamina is depleted for each attack or spell cast
           * melee and ranged damage is 33% higher than normal mode
           * spellpower is 33% higher than normal mode
       * escape mode (press 'S' if in another mode)
           * movement costs stamina
           * stealth is reduced to 25% of normal
           * evasion is increased 33% over normal
           * movement speed is 1.0 for most species
               * 0.7 for spriggan
               * 0.85 for centaur
               * 1.4 for naga
           * movement speed is penalized if the player stops or changes direction more than 45 degrees from original heading
       * focus mode (press 'E' if in another mode)
           * attacks, spells cast, and movement cost stamina
           * spell hunger -> stamina cost is doubled
           * stealth is increased 33% over normal
           * accuracy is increased 33% over normal
           * spell failure chance is reduced (-10% or 1/2, whichever is worse)
   * exertion mode examples:
       * standard difficulty, normal mode damage: 100% of vanilla
       * standard difficulty, power mode damage: 133% of vanilla
       * nightmare difficulty, normal mode damage: 50% of vanilla
       * nightmare difficulty, power mode damage: 66% of vanilla
   * when stamina is depleted, game switches back to normal exertion mode automatically
   * species differences:
       * centaurs and nagas have a larger than normal stamina pool
       * spriggans consume half as much stamina when running
       * trolls consume stamina more quickly than normal, but immediately eat 50% of corpses dropped to recover some stamina.
       * ghouls immediately consume 50% of corpses dropped to recover some health / remove rot.
   * vampiric weapons consume stamina as they heal the player, and stop healing the player when stamina runs out
   * eating fruit recovers 25 stamina
   * eating royal jelly reduces stamina costs to 0 temporarily

Minor background tweaks
   * Fighters no longer have a special exception to prevent them from choosing a quarterstaff.
   * Skald, Warper, and Arcane Marksman have some minor improvements to their starting package. The Arcane Marksman in particular has a much better spellbook.

Remove food
   * no more chopping or eating
   * fruits and royal jelly can be consumed with the quaff command
       * consume fruit to regain stamina! (also used by Fedhas)
       * consum royal jelly to reduce stamina costs by 75% !
   * vampire still drink blood to change their satiation level
   * ghouls automatically eat some of the corpses they kill to heal and cure rot

Enchanted Forest and Dwarven Halls (now called Dwarven Fortress) brought back
   * Work continues to make these more interesting and fun!
   * If you visit these areas, your feedback is very valuable.
   * If you don't want to play in a likely broken or boring area, avoid these branches for now.
   * If you want to help test and refine them, go ahead and explore, and tell me what you think will make them better.
   * Dwarf will eventually be 5 floors
   * Dwarf and Forest are intended to be late game challenges that are substantially harder than vaults and Pan. It will take some work to get them to this point, but that's the direction I want to take them.

v1.2.3

Experience modes
   * You can now configure the experience settings through predefined modes. The old settings have been removed.
   * You select a mode by using "experience_mode = ..." in your rc file.
   * Current modes:
       * experience_mode = classic
           * this is the basic vanilla crawl way
           * experience is gained by killing monsters
       * experience_mode = simple_xl
           * experience is only gained by reaching new floors.
           * every 3 floors gives a new experience level, assuming a species exp apt of 0, and difficulty mode is normal
       * experience_mode = simple_depth
           * experience is only gained by reaching new floors.
           * depending on how difficult your current level is determines how much experience you get for reaching it.
       * experience_mode = balance
           * 1 experience potion spawns on each floor
           * experience potions are dropped by each unique and player ghost
           * experience is evenly divided between reaching new floors, killing monsters, and drinking potions at the lowest possible level
       * experience_mode = serenity
           * like balance, but calmer
               * with serenity, as opposed to balance, drinking a potion advances your experience based on your current xl, not based on the floor, so you can drink them immediately, or save a few for when you need to clear out some draining fast.
           * 1 experience potion spawns on each floor
           * experience potions are dropped by each unique and player ghost
           * experience is evenly divided between reaching new floors, killing monsters, and drinking potions
           * strategy: you want to delay drinking potions or going down floors for as long as possible, and kill everything possible, since the amount of experience you get from floors and potions is based on your current xl. But don't wait too long or you will be too underpowered to survive.
       * experience_mode = intensity
           * like balance, but more intense
           * 1 experience potion spawns on each floor
           * experience potions are dropped by each unique and player ghost
           * no experience gained for killing monsters
           * a little experience is gained for reaching a new floor
           * but most of the experience is gained from drinking potions at the lowest dungeon level possible
       * experience_mode = pacifist
           * 1 experience potion spawns on each floor
           * experience is gained by drinking potions and reaching new branch floors
           * experience is lost for each monster killed
       * experience_mode = destroyer
           * experience is gained for killing monsters
           * experience potions are dropped by each unique and player ghost
           * experience is lost for each new floor reached
           * experience potions give experience based on player xl
           * a good approach here is to clear all monsters from each floor, but delay drinking exp potions for a long as possible, since the amount of xp they give will be higher if the player is at a higher xl.

v1.2.2

Human
   * exp apt +2
   * fighting and spellcasting apt +2
   * can crosstrain more skills
       * fire, ice, earth, air
       * invo, evo, spellcasting
   * god wrath lasts 1/4 as long, since the gods understand how fickle humans can be.

Draconian
   * Pale Draconian
       * inv apt 1 -> 0
       * evo apt 1 -> 2
   * Mottled Draconian
       * now has rF+ and rC+
       * sticky flame can go 2 spaces instead of 1
       * +1 apt for elemental magics and poison

Options for experimenting with experience model changes (each option is shown with it's current default)
   * level_27_cap = false
       * when false, the level and skill cap is 99, and a smooth scale is used to determine how much exp is required for each level
       * when true, the old experience cap and scale is in place

v1.1

Better low health warnings, especially for new players
   * A new option, called danger_mode_threshold was made which defaults to 30.
   * If damage is done to the player exceeding danger_mode_threadhold percent of the player's current hp, danger mode is switched to on, and a special warning message is given, the screen flashes, and the player is given a "more" message. This message appears only if danger mode is off.
   * Danger mode stays on until no "danger" events have happened for 10 turns.
   * Example: A player has 20 health left, the danger_mode_threshold is set to 20, the player isn't currently in danger mode, and he gets hit for 5 damage. 5 is less than 30% of 20, so no warning message is given. Then the player is hit for 5 points of damage again, and since 5 damage is more than 30% of 15, the danger message is given, and danger mode is turned on. No more warnings will be given until things quiet down.
   * This is disabled by setting danger_mode_threshold to 0.

Curse enhancement
   * Curses now have a curse level.
   * The curse on equipped items decay as a player gains experience (unless the player worships Ash of course).
   * Typical early game curses are at curse level 100.
   * Reading a remove curse scroll reduces the curse level by a minimum of 100, scaling up with invocations and piety (whichever is greater).
   * Once the curse level drops to 0, the item is no longer cursed.
   * Later game curses may be much higher than 100, and require multiple scrolls or a lot of waiting before they go away.
   * Mummy death curses curse an equipment slot, not an item. Equipment slot curses pass on to an item as soon as it is equipped.
   * The same item can be cursed multiple times, increasing the curse level.
   * Curse level > 1000 will cause the bonuses of that item to be neutralized. A +5 long sword of poison becomes a +0 long sword, while the curse is above 1000.

v1.0

Inventory expansion
   * The inventory has been divided into two groups: consumables (potions, scrolls, and food), and everything else. The 'i' command shows the weapons, armour, evokables, etc. The 'I' command shows the consumables. Each can have 52 items.
   * The drop command has been split into two also: 'd' to drop inventory items, control-D to drop consumable items.
   * The adjust command '=' can now also be applied to the (c)onsumables.
   * The spell list command has been moved from 'I' to 's'.
   * Wands automatically stack in inventory, possibly exceeding max capacity.

Stair changes
   * Monsters can't use stairs, but if they hurt you while going up or down stairs, you are interrupted.
       * Eliminates ability of player to pull apart groups of monsters into bite size pieces.
       * Eliminates easy escapes on stairs when monsters can still inflict damage (even ranged ones).
   * If you take the stairs, but then immediately go back through the stairs again, you can't be interrupted.
       * Avoids the situation where a player goes onto a level surrounded by dangerous monsters, but can't escape back to the previous level.

Damage numbers are shown
   * yeah I know it adds some details that the original designers want to avoid. But really, how many crawl players aren't seriously hard core RPGers anyway? I personally find it a bit less tedious to try out different attack strategies when I can see the numbers, instead of trying to guess from vague descriptions whether or not a change is actually improving my attacking effectiveness.

Game difficulty levels
   * At game start, or in the init file, you can specify that the game is easy, normal, or hard.
   * Normal is the standard, unmodified parameters.
   * Easy:
       * reduces the chances of out of depth monsters spawning
       * player has 50% more hp
       * player has 50% more mp
       * player has 33% more sp
       * increases the amount of gold spawned by 33%
       * starts player with a healing potion
       * faster level advancement (exp apt + 2)
       * 1/4 normal score
       * healing potions heal 100% of health
       * ghosts don't spawn above level 10 of dungeon
   * Normal:
       * ghosts don't spawn above level 5 of dungeon
       * healing potions heal 50% of health, minimum of 20 points
   * Hard:
       * increases the chances of out of depth monsters spawning
       * slower level advancement (exp apt - 2)
       * 33% less gold spawned
       * 33% less hp
       * 33% less mp
       * 25% less sp
       * 4x normal score
       * healing potions heal 25% of health, minimum of 20 points
   * This is just the beginning. I'm sure with more testing and experience, we can find ways to make these difficulty levels a lot more interesting.

Unequipping shields
   * When trying to equip a weapon that is incompatible with a shield, the game will ask if you want to unequip the shield first, saving some hassel.

Traps
   * Shafting doesn't happen in the first 2 floors of dungeon

Formicid
   * Immune to shaft traps
   * Fast movement
   * Vulnerable to poison

Djinni brought back from the dead and greatly enhanced.
   * Unusual contamination mechanism that they originally had has been removed. They also consume food like normal species, so excessive spell casting has the normal consequence.
   * Djinni can no longer read scrolls of any kind, even though they can use spell books.
   * Since they can't read remove curse scrolls, cursed items are a much bigger deal for them. To mitigate this a little, they have a remove curse ability that costs one permanent MP.
   * Since they can't read identify scrolls, they can either identify items by using them (very dangerous for potentially cursed items), or wait and use the new insight mutation that they start with which, over time, randomly identifies attributes of items in their inventory. This mutation can also randomly be gained by other species as they drink mutation potions, etc. An item with an earlier inventory letter will likely be identified before an item with a later one ('b' will be identified sooner than 'z' in most cases).
   * Fire damage heals them. Standing in a flame cloud is a way to heal, but the flame cloud is absorbed by the Djinni and disappears more quickly than normal. Be careful though, increasing their fire resistance (through an item, ring, etc), will reduce their healing. Maybe you can find a source of lowering their fire resistance?
   * They have very low magic resistance.
   * They have the glow mutation, which increases with experience, which reduces their stealth.
   * Cold and water damage does triple the resistable amount of damage, in addition to the penalties of having one level of cold vulnerability. A ring of ice would be very valuable here, reducing the cold damage done, as well as increasing the healing from fire damage. Unfortunately, there is no water resistance buff... muahahaha....
   * Unarmed combat gets a fire brand.
   * They start with ephemeral 3 mutation, and slowly lose it as they gain experience (as they become more corporeal). The ephemeral mutation give a chance that attacks (both melee and ranged) will completely pass through the Djinni.
   * Dithmenos rejects Dj of course

Mummy
   * initial attributes improved greatly, making early game much easier, but long term they are weaker, gaining levels more slowly than before
   * immunity to curses, so trying out cursed armour or weapons isn't a problem
   * can worship Ash, but their curse immunity leaves

Felid
   * can wear 4 rings. Why not? They have four identical "hands"... I find that this helps to balance their severe limitations a bit better and make the early game a bit more doable.
   * move faster as they advance in levels.
   * have exactly 9 lives right at the beginning, which never increases. How many lives can you end up with at the end of the game?
   * no loss of experience when death happens

Ghouls
   * ghouls automatically eat some of the corpses they kill to heal and cure rot

Halfling
   * added wild magic mutation at the level 8, 16, and 24, making it much more difficult for them to cast spells, but they are quite powerful when they do.
   * added an extraordinary level of magic resistance
   * improved conjuration apt from -2 to +2

Kobold
   * added permanent evolution mutation. Although this sounds similar to how demonspawns work, it actually plays quite differently, because the player will occasionally get random bad mutations, and sometimes have to adjust their play style to compensate. There is also a greater than normal chance that evolve will replace a bad mutation with a good, making drinking mutation potions a safer strategy for kobolds. Unlike demonspawn which get permanent mutations, the mutations a kobold gets through evolve are temporary, making cure mutation potions often a very bad thing. And of course they only get ordinary mutations, not the cool demonspawn specific ones.
   * They start with evolve 2, but after reaching xl 10, they lose one level of it.
   * as a result of their highly mutable nature, they cannot worship Zin.

Sludge Elf
   * brought back because I needed a species to experiment with.
   * added subdued magic 3 mutation at the beginning, making it very easy for them to cast spells, but they are quite weak as a result. This enables them to cast high level, low power requirement spells early or even in heavy armour, that would be impossible for others.
   * their starting attributes are unusually low.
   * they have low mana.

Naga
   * poison spitting advances to level 2 at XL 6, and to level 3 at XL 12.
   * made them even slower, but the slowness comes gradually, at levels 1, 4, and 8.

Ogre
   * [powered by pain](http://crawl.chaosforge.org/Powered_By_Pain) at level 12.
   * powered by pain now only works 2/3 of the time on normal, and 1/3 of the time on hard.
   * significantly increased health. They now have by far the greatest health pool of all species.
   * Poor fitting armour now reduces base AC by 1/3, and level 2 reduces it by 2/3. They get level 1 at exp level 10, and level 2 of this effect at exp level 15.

Octopode
   * [Aug](http://crawl.chaosforge.org/Augmentation) 1 at level 6. This boosts damage and spellpower when at high health. Then their gelatinous body advances to level 3 by exp level 8.

Tengu
   * while flying, their speed increases based on their experience level.
   * start with shock resistance
   * start with thin skeletal structure
   * can't wear rings at all
   * a new mutation called enchantment absorption which can cause enchantments used on the player to instead be absorbed as mana
       * at 6 they get enchantment absorb 1: they absorb 10% of all bad enchantments
       * at 12 they get ench absorb 2: they absorb 40% of all bad enchantments
       * at 18 they get ench absorb 3: absorb 90% of all bad enchantments, making them almost immune.

Trolls
   * Cave Troll
       * like old troll, except with a couple more shaggy fur mutations
   * Moon Troll
       * gain several elemental resistances as they gain levels
       * start with higher hp than normal, but never gain hp with level. They can increase hp by training fighting or through items, but late game they will have rather low hp.
       * better magic aptitudes, but spellcasting skill is still bad
       * apt 0 defensive skills (to make up for their low late-game health).

Deep Dwarf
   * recharge ability split: lesser recharge (costs 1 PMP), and greater recharge (costs 3 PMP). Use greater recharge to recharge wands of healing, haste, and teleport

Deep Elf
   * increased spellcasting aptitude
   * increased mana

Demigod
   * Gain str/int/dex even faster
   * Slower leveling

Lava Orcs
   * Please see the [wiki](https://github.com/jeremygurr/dcssca/wiki/Lava-Orc) for full details.

Vine Stalkers
   * They can lignify or unlignify at will, which takes time but may add some interesting strategic choices.

Spells
   * You can see the spell list with the 'S' command.
   * It is a unified spell list which removes the need to toggle between two views.
   * All spells can have 200 spell power
   * spell casting skill has a greater effect on spell success and power.
   * Throw Flame is not in spellbooks. Monsters cast it, but the player can't. We enhanced Flame Tongue to fill that void. Magma Bolt is in the Flames Book as a level 5 spell alongside Fireball.

Potions and wands of heal wounds
   * easy mode: minimum healing of 20
   * normal mode: minimum healing of 10 (same as vanilla crawl)
   * hard mode: minimum healing of 5

Recharge scrolls
   * less common, but fully charges the wand.
   * recharging cuts a wand's max charge capacity in half.

Potions of magic: 40 mp, 30 mp, or 20 mp at standard/challenge/nightmare difficulty

Potions of lignification (and lignify ability of vinestalkers)
   * initial AC is 15
   * AC scales with experience (1.5 AC per XL)
   * 2x health boost
   * intention is to make it useful even in late game. 

Mutation tweaks
   * powered by pain works the same on easy, 2/3 as often on normal, and 1/3 as often on hard.

Gozag
   * Only half of corpses dropped change to gold, so trolls can get their dragon hides. There is more gold dropped per corpse to make the total gold the same as before.

Minor item tweaks
   * ring of magical power now does crazy stuff. Ask about it on IRC!http://crawl.chaosforge.org/XL
