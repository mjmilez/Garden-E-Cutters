import sys
from pathlib import Path

# Add parent directory to path so we can import from src
sys.path.insert(0, str(Path(__file__).parent.parent))


import pytest
from src.create_data import create_data, calculate_checksum, dd_to_ddmm_high_precision

def test_high_precision_format():
    """Verify 7 decimal places in minutes."""
    result = dd_to_ddmm_high_precision(29.668724, is_longitude=False)
    assert len(result.split('.')[1]) == 7
    assert result.startswith("29")

def test_checksum_calculation():
    """Verify NMEA checksum is correct."""
    sentence = "GPGGA,123519,2940.1234567,N,08219.7654321,W,4,20,0.8,46.123,M,-34.000,M,1.2,0101"
    checksum = calculate_checksum(sentence)
    assert len(checksum) == 2
    assert checksum.isalnum()

def test_gga_output_format():
    """Test GGA sentence structure."""
    gen = create_data(sample_rate=1, output_format="GGA")
    sentence = next(gen)
    
    assert sentence.startswith("$GPGGA")
    assert sentence.count(',') >= 14
    assert '*' in sentence
    assert len(sentence.split('*')[1]) == 2  # Checksum is 2 chars

def test_rtk_fixed_quality():
    """Verify RTK fixed produces quality 4."""
    gen = create_data(rtk_state="fixed", output_format="GGA")
    sentence = next(gen)
    parts = sentence.split(',')
    assert parts[6] == '4'  # Fix quality field

def test_rtk_float_quality():
    """Verify RTK float produces quality 5."""
    gen = create_data(rtk_state="float", output_format="GGA")
    sentence = next(gen)
    parts = sentence.split(',')
    assert parts[6] == '5'