import random

class Comparator:
    def __init__(self):
        # (name, watts) -> Geräte mit typischer Dauerleistung in Watt
        self.consumers_in_watts = [
            ("toaster", 800),
            ("microwave", 1200),
            ("refrigerator", 150),
            ("air_conditioner", 2000),
            ("playstation", 200),
            ("laptop", 50),
            ("light_bulb", 10),
            ("led_light_bulb", 8),
            ("desktop_pc", 300),
            ("gaming_pc_high_end", 650),
            ("television_led_55zoll", 120),
            ("washing_machine", 2000),
            ("dryer", 3000),
            ("dishwasher", 1800),
            ("vacuum_cleaner", 1400),
            ("hair_dryer", 1800),
            ("electric_kettle", 2200),
            ("coffee_machine", 1000),
            ("router_wifi", 10),
            ("smartphone_charger", 5),
            ("ceiling_fan", 75),
            ("space_heater", 2000),
            ("iron", 1200),
            ("electric_oven", 2400),
            ("hob_induction", 3000),
            ("aquarium_pump", 15),
            ("server_rack_1u", 400),
            # wtf?
            ("disco_ball_motor", 20),
            ("fog_machine", 900),
            ("massage_chair", 150),
            ("popcorn_machine", 1200),
            ("robot_vacuum_charging", 60),
            ("electric_scooter_charger", 250),
            ("giant_inflatable_dinosaur_fan", 250),
            ("retro_arcade_machine", 300),
            ("chocolate_fountain", 200),
            ("nose_hair_trimmer", 3),
            ("electric_toothbrush_charger", 2),
            # weitere sinnvolle Haushalts- und Elektrogeräte
            ("freezer_chest", 200),
            ("water_heater_boiler", 2000),
            ("electric_shower_head", 9000),
            ("radiator_electric_oil", 1500),
            ("floor_heating_per_room", 800),
            ("dehumidifier", 300),
            ("humidifier", 30),
            ("air_purifier", 50),
            ("printer_laser", 500),
            ("printer_inkjet", 20),
            ("monitor_27zoll", 40),
            ("router_modem_combo", 15),
            ("network_switch_8port", 8),
            ("nas_server_2bay", 30),
            ("game_console_xbox_series_x", 180),
            ("nintendo_switch_docked", 18),
            ("soundbar", 30),
            ("electric_blanket", 100,),
            ("towel_warmer", 150),
            ("garage_door_opener", 350),
            ("power_drill", 700),
            ("circular_saw", 1400),
            ("angle_grinder", 900),
            ("lawn_mower_electric", 1400),
            ("leaf_blower_electric", 2500),
            ("pressure_washer", 1800),
            ("pool_pump", 750),
            ("sewing_machine", 100),
            ("ev_charger_wallbox_11kw", 11000),
            ("ev_charger_wallbox_22kw", 22000),
            ("heat_pump_dryer", 1000),
            ("wine_cooler", 100),
            ("electric_bike_charger", 120),
            ("blender", 500),
            ("juicer", 400),
            ("bread_maker", 600),
            ("rice_cooker", 500),
            ("slow_cooker", 200),
            ("air_fryer", 1500),
            ("waffle_iron", 1200),
            ("sandwich_maker", 750),
        ]

    def compare_consumption(self, consumption_in_kwh):
        close_devices = []
        consumption_in_watthours = consumption_in_kwh * 1000  # Convert kWh
        min_time_seconds = 10

        for device, watts in self.consumers_in_watts:
            if consumption_in_watthours >= watts * min_time_seconds / 3600:
                time_seconds = consumption_in_watthours / watts * 3600
                close_devices.append((device, time_seconds))

        #print(close_devices)
        compare_device = close_devices[random.randint(0, len(close_devices) - 1)]
        #print(f"Consumption: {consumption_in_kwh:.6f} kWh is similar to {compare_device[0]} running for {self.sensible_time_output(compare_device[1])}.")

        #print(f"Consumption: {consumption_in_kwh} kWh is similar to {compare_device[0]} running for {compare_device[1]} seconds.")

        return compare_device[0], self.sensible_time_output(compare_device[1])

    def sensible_time_output(self, time_seconds):
        if time_seconds < 60:
            return f"{time_seconds:.2f} seconds"
        elif time_seconds < 3600:
            minutes = time_seconds / 60
            return f"{minutes:.2f} minutes"
        else:
            hours = time_seconds / 3600
            return f"{hours:.2f} hours"

    