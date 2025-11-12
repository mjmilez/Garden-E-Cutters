Create src/create_data.py

## Core Requirements:

Function signature: create_data(sample_rate, origin_lat, origin_lon, noise_level, rtk_state, output_format)

## Parameters:

sample_rate: Hz (e.g., 1, 5, 10)
origin_lat, origin_lon: Decimal degrees for starting position
noise_level: meters (e.g., 0.01 for RTK, 2.5 for standard GPS)
rtk_state: "fixed" (quality=4) or "float" (quality=5)
output_format: "GGA", "RMC", "JSON", or "binary"

## Must Generate:

Valid NMEA checksum
High precision format (7 decimal places in minutes): DDMM.MMMMMMM
Realistic timestamps (UTC)
Appropriate fix quality (4=RTK fixed, 5=RTK float)
Satellite count (12-24 realistic range)
HDOP values (0.6-1.2 for RTK)
Age of differential (0.5-3.0 seconds typical)
Base station ID (configurable, default 101)