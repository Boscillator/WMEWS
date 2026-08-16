import argparse
from pathlib import Path

def parse_args():
    parser = argparse.ArgumentParser(
            prog="extract-features.py",
            description="Produce chuncked ML features from raw WMEWS data")
    parser.add_argument("input", help="Input directory", type=Path)
    parser.add_argument("output", help="Output directory", type=Path)
    return parser.parse_args()

def main():
    args = parse_args()
    print(args)

if __name__ == '__main__':
    main()

