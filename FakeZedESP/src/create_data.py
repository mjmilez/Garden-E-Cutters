# src/create_data.py
import time
import math
import random
from datetime import datetime

def create_data(sample_rate=1, origin_lat=29.668724, origin_lon=-82.329572,
                noise_level=0.02, rtk_state="fixed", output_format="GGA"):
    """
    Generate synthetic GNSS/RTK data for testing.
    
    Args:
        sample_rate: Samples per second (Hz)
        origin_lat: Origin latitude (decimal degrees)
        origin_lon: Origin longitude (decimal degrees)
        noise_level: Position noise in meters
        rtk_state: "fixed" or "float"
        output_format: "GGA", "RMC", "JSON", or "binary"
    
    Yields:
        Formatted GNSS data string or dict
    """
    
    # Convert RTK state to fix quality
    fix_quality = 4 if rtk_state == "fixed" else 5
    
    # Realistic parameters
    num_sats = random.randint(18, 24)
    hdop = random.uniform(0.6, 1.2)
    base_id = 101
    
    while True:
        # Add noise to position
        lat_noise = random.gauss(0, noise_level) / 111320  # meters to degrees
        lon_noise = random.gauss(0, noise_level) / (111320 * math.cos(math.radians(origin_lat)))
        
        current_lat = origin_lat + lat_noise
        current_lon = origin_lon + lon_noise
        
        # Generate timestamp
        now = datetime.utcnow()
        utc_time = now.strftime("%H%M%S.%f")[:-4]  # HHMMSS.SS
        
        if output_format == "GGA":
            yield generate_gga(utc_time, current_lat, current_lon, 
                              fix_quality, num_sats, hdop, base_id)
        elif output_format == "JSON":
            yield generate_json(utc_time, current_lat, current_lon,
                               fix_quality, num_sats, hdop, base_id)
        
        time.sleep(1.0 / sample_rate)

def dd_to_ddmm_high_precision(decimal_degrees, is_longitude=False):
    """Convert decimal degrees to DDMM.MMMMMMM format (7 decimal places)."""
    degrees = int(abs(decimal_degrees))
    minutes = (abs(decimal_degrees) - degrees) * 60
    
    if is_longitude:
        return f"{degrees:03d}{minutes:010.7f}"  # DDD MM.MMMMMMM
    else:
        return f"{degrees:02d}{minutes:010.7f}"  # DD MM.MMMMMMM

def calculate_checksum(sentence):
    """Calculate NMEA checksum (XOR of all bytes between $ and *)."""
    checksum = 0
    for char in sentence:
        checksum ^= ord(char)
    return f"{checksum:02X}"

def generate_gga(utc_time, lat, lon, fix_quality, num_sats, hdop, base_id):
    """Generate NMEA GGA sentence with high precision."""
    
    lat_ddmm = dd_to_ddmm_high_precision(lat, is_longitude=False)
    lon_ddmm = dd_to_ddmm_high_precision(lon, is_longitude=True)
    lat_hem = "N" if lat >= 0 else "S"
    lon_hem = "E" if lon >= 0 else "W"
    
    altitude = random.uniform(45, 47)
    geoid_sep = -34.0
    age_diff = random.uniform(0.5, 2.5)
    
    # Build sentence without checksum
    sentence = (f"GPGGA,{utc_time},{lat_ddmm},{lat_hem},{lon_ddmm},{lon_hem},"
                f"{fix_quality},{num_sats},{hdop:.1f},{altitude:.3f},M,"
                f"{geoid_sep:.3f},M,{age_diff:.1f},{base_id:04d}")
    
    checksum = calculate_checksum(sentence)
    return f"${sentence}*{checksum}"

def generate_json(utc_time, lat, lon, fix_quality, num_sats, hdop, base_id):
    """Generate JSON format output."""
    lat_ddmm = dd_to_ddmm_high_precision(lat, is_longitude=False)
    lon_ddmm = dd_to_ddmm_high_precision(lon, is_longitude=True)
    lat_hem = "N" if lat >= 0 else "S"
    lon_hem = "E" if lon >= 0 else "W"
    
    return {
        "utc": utc_time,
        "lat_ddmm": lat_ddmm,
        "lat_hem": lat_hem,
        "lon_ddmm": lon_ddmm,
        "lon_hem": lon_hem,
        "fix_quality": fix_quality,
        "sats": num_sats,
        "hdop": hdop,
        "alt_m": random.uniform(45, 47),
        "age_s": random.uniform(0.5, 2.5),
        "base_id": base_id
    }