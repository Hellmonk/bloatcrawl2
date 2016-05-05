// can only vent if you're at max heat
    if (temperature() = TEMP_MAX)
        _add_talent(talents, ABIL_VENT_LAVA, check_confused);
 
 // range       
 case ABIL_VENT_LAVA: return 2;

// fail chance, mana cost        
    { ABIL_VENT_LAVA, "Vent Lava",
        0, 3, 0, 0, {FAIL_XL, 20, 1}, abflag::BREATH },
        
// the actual effects
        case ABIL_VENT_LAVA:
        {
            
            int power = (you.strength + you.intelligence) / 2;

            string msg = "You erupt, spewing lava!";

            if (!zapping(ZAP_VENT_LAVA, power, beam, true, msg.c_str()))
                return SPRET_ABORT;
            break;

            you.temperature = TEMP_MIN;
            you.stamina -= 75;        
        }
