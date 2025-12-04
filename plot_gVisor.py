import matplotlib.pyplot as plt
import numpy as np
import argparse

def plot(p0, p1, std0, std1, title, out):
    cycles = [p0, p1]
    stds = [std0, std1]
    labels = ['Page 0', 'Page 1']
    x = np.arange(len(labels))

    plt.figure()
    plt.bar(x, cycles, yerr=stds, capsize=5)
    # plt.yscale('log')  # log axis; yerr remains additive in data units
    plt.xticks(x, labels)
    plt.ylabel("Cycles")
    plt.title(title)

    plt.tight_layout()
    plt.savefig(out, dpi=300)
    print(f"Saved {out}")

def parse_args():
    parser = argparse.ArgumentParser()
    
    parser.add_argument("--p0", type=float, required=True, help="Page 0 average cycle count")
    parser.add_argument("--p1", type=float, required=True, help="Page 1 average cycle count")
    parser.add_argument("--std0", type=float, required=True, help="Page 0 cycle standard deviation")
    parser.add_argument("--std1", type=float, required=True, help="Page 1 cycle standard deviation")
    parser.add_argument("--title", type=str, required=True, help="Plot title")
    parser.add_argument("--out", type=str, required=True, help="Output filename")
    
    return parser.parse_args()

def main():
    args = parse_args()
    plot(args.p0, args.p1, args.std0, args.std1, args.title, args.out)

if __name__ == "__main__":
    main()
