import random

class Comparator:
    def __init__(self):
        # (name, watts) -> devices with typical continuous power draw in watts
        self.consumers_in_watts = [
            ("Toaster", 800),
            ("Microwave", 1200),
            ("Refrigerator", 150),
            ("Air Conditioner", 2000),
            ("PlayStation", 200),
            ("Laptop", 50),
            ("Light Bulb", 10),
            ("LED Light Bulb", 8),
            ("Desktop PC", 300),
            ("High-End Gaming PC", 650),
            ("55-Inch LED TV", 120),
            ("Washing Machine", 2000),
            ("Clothes Dryer", 3000),
            ("Dishwasher", 1800),
            ("Vacuum Cleaner", 1400),
            ("Hair Dryer", 1800),
            ("Electric Kettle", 2200),
            ("Coffee Machine", 1000),
            ("WiFi Router", 10),
            ("Smartphone Charger", 5),
            ("Ceiling Fan", 75),
            ("Space Heater", 2000),
            ("Iron", 1200),
            ("Electric Oven", 2400),
            ("Induction Hob", 3000),
            ("Aquarium Pump", 15),
            ("1U Server Rack", 400),
            # wtf?
            ("Disco Ball Motor", 20),
            ("Fog Machine", 900),
            ("Massage Chair", 150),
            ("Popcorn Machine", 1200),
            ("Robot Vacuum (Charging)", 60),
            ("E-Scooter Charger", 250),
            ("Giant Inflatable Dinosaur Fan", 250),
            ("Retro Arcade Machine", 300),
            ("Chocolate Fountain", 200),
            ("Nose Hair Trimmer", 3),
            ("Electric Toothbrush Charger", 2),
            # more sensible household and electronic devices
            ("Chest Freezer", 200),
            ("Water Heater/Boiler", 2000),
            ("Electric Shower Head", 9000),
            ("Electric Oil Radiator", 1500),
            ("Underfloor Heating (per room)", 800),
            ("Dehumidifier", 300),
            ("Humidifier", 30),
            ("Air Purifier", 50),
            ("Laser Printer", 500),
            ("Inkjet Printer", 20),
            ("27-Inch Monitor", 40),
            ("Router/Modem Combo", 15),
            ("8-Port Network Switch", 8),
            ("2-Bay NAS Server", 30),
            ("Xbox Series X", 180),
            ("Nintendo Switch (Docked)", 18),
            ("Soundbar", 30),
            ("Electric Blanket", 100,),
            ("Towel Warmer", 150),
            ("Garage Door Opener", 350),
            ("Power Drill", 700),
            ("Circular Saw", 1400),
            ("Angle Grinder", 900),
            ("Electric Lawn Mower", 1400),
            ("Electric Leaf Blower", 2500),
            ("Pressure Washer", 1800),
            ("Pool Pump", 750),
            ("Sewing Machine", 100),
            ("11 kW EV Wallbox Charger", 11000),
            ("22 kW EV Wallbox Charger", 22000),
            ("Heat Pump Dryer", 1000),
            ("Wine Cooler", 100),
            ("E-Bike Charger", 120),
            ("Blender", 500),
            ("Juicer", 400),
            ("Bread Maker", 600),
            ("Rice Cooker", 500),
            ("Slow Cooker", 200),
            ("Air Fryer", 1500),
            ("Waffle Iron", 1200),
            ("Sandwich Maker", 750),
        ]

    def compare_consumption(self, consumption_in_kwh):
        close_devices = []
        consumption_in_watthours = consumption_in_kwh * 1000  # Convert kWh
        min_time_seconds = 10
        #max_time_seconds = 36000  # 10 hours

        for device, watts in self.consumers_in_watts:
            required_time_seconds = consumption_in_watthours / watts * 3600
            if min_time_seconds <= required_time_seconds: # <= max_time_seconds:
                close_devices.append((device, required_time_seconds))

        if not close_devices:
            # Fallback: no device falls within the sensible time window,
            # just compare against every device instead of crashing.
            close_devices = [
                (device, consumption_in_watthours / watts * 3600)
                for device, watts in self.consumers_in_watts
            ]

        #print(close_devices)
        compare_device = close_devices[random.randint(0, len(close_devices) - 1)]
        #print(f"Consumption: {consumption_in_kwh:.6f} kWh is similar to {compare_device[0]} running for {self.sensible_time_output(compare_device[1])}.")

        #print(f"Consumption: {consumption_in_kwh} kWh is similar to {compare_device[0]} running for {compare_device[1]} seconds.")

        return compare_device[0], self.sensible_time_output(compare_device[1])

    def sensible_time_output(self, time_seconds):
        total_seconds = round(time_seconds)

        if total_seconds < 60:
            second_suffix = "s" if total_seconds != 1 else ""
            return f"{total_seconds} second{second_suffix}"
        elif total_seconds < 3600:
            minutes, rest_seconds = divmod(total_seconds, 60)
            second_suffix = "s" if rest_seconds != 1 else ""
            minute_suffix = "s" if minutes != 1 else ""
            return f"{minutes} minute{minute_suffix} and {rest_seconds} second{second_suffix}"
        elif total_seconds < 86400:
            hours, rest_seconds = divmod(total_seconds, 3600)
            rest_minutes = rest_seconds // 60
            minute_suffix = "s" if rest_minutes != 1 else ""
            hour_suffix = "s" if hours != 1 else ""
            return f"{hours} hour{hour_suffix} and {rest_minutes} minute{minute_suffix}"
        else:
            days, rest_seconds = divmod(total_seconds, 86400)
            rest_hours = rest_seconds // 3600
            day_suffix = "s" if days != 1 else ""
            hour_suffix = "s" if rest_hours != 1 else ""
            return f"{days} day{day_suffix} and {rest_hours} hour{hour_suffix}"

    