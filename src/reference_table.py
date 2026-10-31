"""
Master reference table for urine test strip color analysis.

IMPORTANT:
The RGB values below are placeholders for software development.
Replace them with calibrated values from the exact test strip
manufacturer's color chart before interpreting real samples.
"""

URINE_REFERENCE_TABLE = {
    "LEUKOCYTES": {
        "unit": "Leukocytes/µL",
        "reading_time_seconds": 120,
        "levels": [
            {
                "label": "Negative",
                "value": 0,
                "rgb": None,
            },
            {
                "label": "Trace",
                "value": 25,
                "rgb": None,
            },
            {
                "label": "Small",
                "value": 75,
                "rgb": None,
            },
            {
                "label": "Large",
                "value": 500,
                "rgb": None,
            },
        ],
    },

    "NITRITE": {
        "unit": "Qualitative",
        "reading_time_seconds": 60,
        "levels": [
            {
                "label": "Negative",
                "value": "Negative",
                "rgb": None,
            },
            {
                "label": "Positive",
                "value": "Positive",
                "rgb": None,
            },
        ],
    },

    "UROBILINOGEN": {
        "unit": "mg/dL",
        "reading_time_seconds": 60,
        "levels": [
            {
                "label": "Normal",
                "value": 0.2,
                "rgb": None,
            },
            {
                "label": "Elevated",
                "value": 2,
                "rgb": None,
            },
            {
                "label": "Elevated",
                "value": 4,
                "rgb": None,
            },
            {
                "label": "Elevated",
                "value": 8,
                "rgb": None,
            },
        ],
    },

    "PROTEIN": {
        "unit": "mg/dL",
        "reading_time_seconds": 60,
        "levels": [
            {
                "label": "Negative",
                "value": 0,
                "rgb": None,
            },
            {
                "label": "Trace",
                "value": "Trace",
                "rgb": None,
            },
            {
                "label": "1+",
                "value": 30,
                "rgb": None,
            },
            {
                "label": "2+",
                "value": 100,
                "rgb": None,
            },
            {
                "label": "3+",
                "value": 300,
                "rgb": None,
            },
            {
                "label": "4+",
                "value": 2000,
                "rgb": None,
            },
        ],
    },

    "PH": {
        "unit": "pH",
        "reading_time_seconds": 60,
        "levels": [
            {"label": "5.0", "value": 5.0, "rgb": None},
            {"label": "6.0", "value": 6.0, "rgb": None},
            {"label": "6.5", "value": 6.5, "rgb": None},
            {"label": "7.0", "value": 7.0, "rgb": None},
            {"label": "7.5", "value": 7.5, "rgb": None},
            {"label": "8.0", "value": 8.0, "rgb": None},
            {"label": "8.5", "value": 8.5, "rgb": None},
        ],
    },

    "BLOOD": {
        "unit": "Qualitative / graded",
        "reading_time_seconds": 60,
        "levels": [
            {
                "label": "Negative",
                "value": "Negative",
                "rgb": None,
            },
            {
                "label": "Trace",
                "value": "Trace",
                "rgb": None,
            },
            {
                "label": "Small",
                "value": "Small",
                "rgb": None,
            },
            {
                "label": "Moderate",
                "value": "Moderate",
                "rgb": None,
            },
            {
                "label": "Large",
                "value": "Large",
                "rgb": None,
            },
        ],
    },

    "SPECIFIC_GRAVITY": {
        "unit": "Specific gravity",
        "reading_time_seconds": 45,
        "levels": [
            {"label": "1.000", "value": 1.000, "rgb": None},
            {"label": "1.005", "value": 1.005, "rgb": None},
            {"label": "1.010", "value": 1.010, "rgb": None},
            {"label": "1.015", "value": 1.015, "rgb": None},
            {"label": "1.020", "value": 1.020, "rgb": None},
            {"label": "1.025", "value": 1.025, "rgb": None},
            {"label": "1.030", "value": 1.030, "rgb": None},
        ],
    },

    "KETONES": {
        "unit": "mg/dL",
        "reading_time_seconds": 40,
        "levels": [
            {"label": "Negative", "value": 0, "rgb": None},
            {"label": "Trace", "value": 5, "rgb": None},
            {"label": "Small", "value": 15, "rgb": None},
            {"label": "Moderate", "value": 40, "rgb": None},
            {"label": "Large", "value": 80, "rgb": None},
            {"label": "Very large", "value": 160, "rgb": None},
        ],
    },

    "BILIRUBIN": {
        "unit": "Qualitative / graded",
        "reading_time_seconds": 30,
        "levels": [
            {"label": "Negative", "value": "Negative", "rgb": None},
            {"label": "Small", "value": "Small", "rgb": None},
            {"label": "Moderate", "value": "Moderate", "rgb": None},
            {"label": "Large", "value": "Large", "rgb": None},
        ],
    },

    "GLUCOSE": {
        "unit": "mg/dL",
        "reading_time_seconds": 30,
        "levels": [
            {"label": "Negative", "value": 0, "rgb": None},
            {"label": "100", "value": 100, "rgb": None},
            {"label": "250", "value": 250, "rgb": None},
            {"label": "500", "value": 500, "rgb": None},
            {"label": "1000", "value": 1000, "rgb": None},
            {"label": "2000+", "value": 2000, "rgb": None},
        ],
    },
}


def display_reference_table():
    """Print all parameters and their reference levels."""

    print("=" * 65)
    print("URINE TEST STRIP MASTER REFERENCE TABLE")
    print("=" * 65)

    for parameter, data in URINE_REFERENCE_TABLE.items():
        print(f"\nParameter: {parameter}")
        print(f"Unit: {data['unit']}")
        print(
            f"Reading time: "
            f"{data['reading_time_seconds']} seconds"
        )

        for level in data["levels"]:
            print(
                f"  {level['label']:<15} "
                f"Value: {level['value']}"
            )


if __name__ == "__main__":
    display_reference_table()
