## v1.5.2

* Retreat simplification (instant rest)
    * Instead of making it optimal for a player to retreat and recharge after every monster killed, the player is 
      instantly fully recharged as soon as they have killed all nearby monsters. 
    * If the player kills the last known monster, and there is another just beyond the edge of LOS, it is brought to
      edge of LOS instead of player instant-resting.
    * If instarest doesn't happen, that means there is an invisible monster nearby or you have seen a monster, 
      but haven't actually killed it, and it has moved out of LOS. In this case you'll have to rest the old-fashioned
      way.

* Rune curse effects are now shown in rune list (show with '{').

* Players can unsummon individual summoned friendlies by using the 'x' command, selecting the monster, and then 
    pressing 'q'.
    
* Players can fire missiles through friendly summons, but when they do, their spell failure is checked for the
  spell that summoned the creature. If it fails, the creature is unsummoned.
    * Friendly fire does not work through other friendlies, such as animated dead and simulacrum. 

* Set hp rot caps based on difficulty.
    * Easy mode can't rot to less than 80% of hp.
    * Standard: 60%
    * Challenge: 40%
    * Nightmare: 20%

* Exhaustion mode is the same as normal mode before. 
    * When you drop below 0 sp or mp, you enter exhaustion mode.
    * In exhaustion mode, your basic attributes are a little lower than normal mode. 
    * You are no longer exhausted once you have at least 5 mp and 5 sp. 
    * There is a chance to rot player hp based on amount of sp or mp spent while exhausted.

* Spells may be cast even when mp is empty.
    * Except for spells which reduce max mp instead of consuming mp (like transmutations and summons).
    
* Focus mode now increases ac, sh, and ev slightly
    * By paying closer attention to incoming attacks, a player can optimize their armour (making sure the enemy hits the
      more fortified places) or increase their chance of dodging the attacks.

* Power mode now doubles cost of action, whether it is MP *or* SP (before it only had an SP cost)
* Focus mode now doubles cost of action, whether it is MP *or* SP (before it only had an MP cost)
    * This means that spellcasters in power mode no longer consume SP when casting a spell, but instead must pay
      double the spell cost. 

* Monster HD slightly adjusted based on difficulty level
    * Easy: -1
    * Standard: 0 (unchanged)
    * Challenge: +1
    * Nightmare: +2

## v1.5.1

* Exertion mode cost is based on action cost instead of a flat cost.
    * For example, if an attack costs 8 SP in normal exertion mode, and the player is in power mode, it will instead
      cost 16 SP (+100%). If they are in focus mode, it would cost 8 SP + 8 MP. 
    * If a player casts a spell costing 10 MP in normal mode, if they were in power mode it would cost 10 MP + 10 SP. In
      focus mode, it would cost 20 MP. 

* Maces have higher base damage, because they have the highest weight of all weapons, and cost the most stamina to use.

* Drinking an experience potion no longer pull up a menu to choose skills. So allocate your training before you drink it.

* Inner Flame is smite targetted now.

* Elyvilon improvements
    * Healing targeting shows pacification chance when a monster is selected.
    * Lesser and Greater healing may be applied to player or monster.
        * When applied to monster, the ability will cost hp from the player.
        * When applied to player, the ability costs piety.

* Each spell costs a minimum of 5 mp, and it's easier to reduce the mp cost than before.

* You can only have a maximum of 5 summons total between all spells.

* Spirit shield
    * Since players have much more magic now, to keep spirit shield from being too powerful, it behaves as if you
      have half as much max mp. So if you have 50 max hp, and 100 max mp, half of your damage would come from hp,
      and half would come from mp (since it acts as if you had 50 mp).

* Unlimited ammo
    * If you fire a ranged weapon without ammo, the unbranded ammo fires.
    * Unbranded ammo no longer spawns.
    * Branded ammo still spawn in higher quantities than before, but always mulches.
    * You can switch to unbranded ammo by hitting the quiver command 'Q', and then '-', just like weilding nothing.

* One shot kill protection
    * Player isn't allowed to receive more than some percentage of damage between times when the player can act.
        * This means, even if paralyzed, there is a limit to how much damage can be done before the player gets
          a chance to do something about it.
    * Nightmare mode: 80% damage per turn max. This means that they are guaranteed to have 1 turn to respond to any
      threat if they are at full health.
    * Challenge mode: 40% damage per turn max. Guarantees 2 turns.
    * Standard mode:  20% damage per turn max. Guarantees 3 turns.
    * Example: in nightmare mode, a player has full health and comes across a pack of death yaks and a basilisk. The
      player has 50 health. The player tries to attack the basilisk, but fails to kill it. The basilisk paralyzes the
      player. The death yaks get a few free rounds of damage to the player while they are paralyzed, causing 100 damage.
      Normally the player would be dead at this point, but since 100 damage is more than 80% of 50, only the 80% damage
      is allowed (40 points of damage) that round. Then the player gets a chance to read a blink scroll and maybe escape.
    * This isn't going to make up for chronically bad habits, but it will reduces the chances of players who are playing
      well from dying because of a freak RNG roll.
    * This may be disabled with: disable_instakill_protection = true

* Labyrinth allows auto-explore
    * It's not super reliable, but I find that is removes some tedium without making it totally trivial.

* Increased Fighting and/or Spellcasting skills for all starting jobs that use them by 1, to make it a little
  easier for characters to get off the ground with the recent changes.

* Orc branch has been doubled in size, to 8 floors
    * The "boss" level is still on floor 4.
    * Floors 5-8 don't give experience for kills, although they will restore drain and charge evocables.

* Turned off monster spawning.
    * Increased initial floor population by 20% to compensate.
    * Monsters still spawn in Abyss and during orb run.

* Deactivated ghosts.
    * I have a better solution, but for now this will help.

* Magic potions restore 40, 30, 20 points depending on difficulty mode.
* Stamina potions restore 40, 30, 20 points depending on difficulty mode.

* Spell factors:
    * Spell failure: dexterity, spellcasting, spell schools
    * Spell power: intelligence, spell schools
    * Spell cost: intelligence, spellcasting

* Removed all food, and created stamina potions. You can now sacrifice stamina potions to evoke fedhas' abilities.
* Quick mode doesn't use stamina if monsters aren't around.

## v1.5

* Stamina overhaul
    * Extremely brief summary: you won't be able to swing around a broad axe as a spriggan with 5 str just as well as
      an ogre with 25 str any more. Trying to swing that broad axe as a spriggan will exhaust the player almost immediately,
      whereas and ogre with some minimal fighting skill (say 3) can use it quite a few times before getting tired. But
      even for an ogre, throwing a large rock is going to be expensive stamina-wise.
    * Normal mode fighting actions cost stamina points (SP).
    * SP cost of attack is based on fighting skill, strength, weapon weight.
        * Heavier weapons will cost more stamina to swing than lighter ones.
    * Spells only cost magic points (MP), no more spell hunger.
    * MP cost is based on spellcasting, intelligence, spell level.
    * You can see how much spells cost in the 'S' spell list.
    * Fixed size MP pool like SP pool. Does not go up with experience level (XL).
    * Power and Focus mode doubles the SP or MP costs of an action.
        * In addition to increasing damage by 33%, power mode now increases attack speed by the same.
    * Careful mode renamed to focus mode.
    * Escape mode was removed and in it's place a separate quick / normal mode
        * This can operate independently of exertion mode, i.e. you can have Quick mode + Focus or Quick + Power mode.
        * Quick mode has the benefits of the old escape mode, which are faster movement speed. But
          there is also an additional stamina cost. Stealth is also dramatically reduced.
        * Normal mode doesn't have those benefits, but it doesn't have the additional stamina cost.
        * Quick mode no longer gives an evasion boost, only a speed boost.
        * Quick mode no longer has a stealth penalty.
        * Quick mode no longer has a movement penalty when changing directions.
    * If you run out of stamina in power mode, and it switches you back to normal mode, once you've recovered 50% of your
      SP, the game will automatically switch you back to power mode. Same applies to Focus mode.
    * Switching exertion modes (normal, power, focus), costs 1 turn.
    * Switching speed mode from normal to quick, costs 0 turns.
    * Switching speed mode from quick to normal, costs 1 turn.

* Summary of exertion modes: (existing info merged with changes from this update)
    * Normal mode
        * Melee and ranged attacks require SP based on weapon weight, player strength, and fighting skill.
            * Weapon skill does not affect SP cost.
        * Spell casting costs MP based on spell level, player intelligence, and spellcasting skill.
            * Spell school skills do not affect MP cost.
        * Damage, accuracy, stealth, spellpower, spell success, and attack speed are reduced from vanilla
          crawl levels, based on difficulty mode being played.
    * Power mode (same as normal mode except for the following)
        * +33% damage
        * +33% spellpower increase
        * +15% attack speed
        * -3 SP per attack or spell cast
    * Focus mode (same as normal mode except for the following)
        * +33% attack accuracy
        * +33% stealth
        * -15% spell failure chance or 1/2 failure chance, whichever is worse
        * -3 MP per movement or attack or spell cast

* Summary of Quick mode: (this can be on or off independently of the player's exertion mode)
    * Slow mode (normal state)
        * Movement delay is 1.1 for all species + slowing down effects, mutations, etc.
            * So a naga still is 1.3 speed because of the slow mutation they possess
    * Quick mode
        * Does NOT cost a turn to activate, but DOES cost a turn to deactivate.
        * Movement delay is based on species, artifact properties, mutations, spells in effect (haste, swiftness)
            * Standard species movement delay in quick mode is 0.9
        * -3 SP per movement
        * SP does not regenerate

* Keyboard changes
    * instead of 's' to open spell list, now you press 'S'
    * 'g' will switch to quick speed mode (go)
    * 's' will switch to normal speed mode (slow)
    * other stamina keys that are the same as the previous version:
        * 'e' turns on power mode. (exert)
        * 'c' changes back to normal mode. (cancel)
        * 'E' turns on focus mode. (E...xamine?)

* Spell failure
    * Spell failure is reduced by intelligence AND dexterity, as well as spellcasting and spell schools.
    * Significant simplification of failure calculations.
    * All spells are between 1 and 99% failure.
    * It's easier to get a level 9 spell down to 80% failure rate than in vanilla.
    * It's harder to get a level 9 spell down to 20% failure rate than in vanilla.
    * If all spell factors are at 10, a player has a 50% spell failure rate for a level 5 spell.
        * So if you've got 0 or 1 in 2 different factors, you can expect to have a hard time even casting level 1 spells.

* New branch layout
    * Dungeon (15 floors)
        * Orcish Mines  (4)
        * Lair (7)
            * Swamp (4)
            * Shoals (4)
            * Spider (4)
            * Snake (4)
        * Forest (7)
            * Crypt (4)
            * Slime (4)
            * Elven Halls (4)
            * Dwarven Fortress (4)
        * Vaults (4)
        * Depths (5)
            * Zig (27)
            * Abyss (4)
            * Vestibule
                * Dis (7)
                * Gehenna (7)
                * Cocytus (7)
                * Tartarus (7)
            * Pandemonium (~25)
            * Zot (4)

