# Here's some frequently needed bits of info for this fork:

## Keyboard

* 'S' expanded spell list
* 'g' quick mode
* 's' slow mode (turns quick mode off)
* 'e' power exertion mode
* 'E' focus exertion mode
* 'c' cancel exertion mode (go back to normal mode)
* 'I' consumable inventory (where your potions, scrolls, and books are)
* '^d' drop something from consumable inventory

## Fighting (perfectly parallels Spellcasting, see below)

* *Damage* is increase by:
    * Strength
    * Weapon Skill

* *Stamina Cost* is reduced by:
    * Strength
    * Fighting Skill

* *Hit Accuracy* is increased by:
    * Dexterity
    * Weapon Skill

* *Attack Speed* is increased by:
    * Dexterity
    * Fighting Skill

* Mindelay is reached when dex+fighting = 50

## Spellcasting (perfectly parallels Fighting, see above)

* *Spell Power* is increased by:
    * Intelligence
    * Spell School Average
    
* *Spell Cost* is reduced by:
    * Intelligence
    * Spellcasting Skill

* *Spell Failure Rate* is reduced by:
    * Dexterity
    * Spell School Average

* *Spell Casting Speed* is increased by:
    * Dexterity
    * Spellcasting Skill

## Exertion modes

* Quick mode
    * Movement speed is either 0.9 or the normal speed of the species, whichever is faster
    * Costs 5 SP per movement
    
* Slow mode (normal speed)
    * Movement speed is 1.1
    
* Power mode
    * Doubles spell and attack costs
    * Increased damage
    * Increased spellpower
    * Increased attack speed and spellcasting speed
    
* Focus mode
    * Doubles spell and attack costs
    * Increased defenses (AC, EV, SH)
    * Increased accuracy
    * Lower spell failure rate
    * Increased stealth
    
