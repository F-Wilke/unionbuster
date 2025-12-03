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
    plt.xticks(x, labels)
    plt.ylabel("Cycles")
    plt.title(title)

    plt.tight_layout()
    plt.savefig(out, dpi=300)
    print(f"Saved {out}")

def plot_diff(p0, p1, title, out):
    p0 = np.array(p0)
    p1 = np.array(p1)

    x = np.arange(len(p0))

    width = 0.35
    plt.figure(figsize=(10, 5))

    plt.bar(x - width/2, p0, width, label="Page 0")
    plt.bar(x + width/2, p1, width, label="Page 1")

    plt.xticks(x, [f"Iter {i}" for i in range(1, 11)])
    plt.ylabel("Cycles")
    plt.title(title)
    plt.legend()

    plt.tight_layout()
    plt.savefig(out, dpi=300)
    print(f"Saved {out}")

def parse_args():
    parser = argparse.ArgumentParser()
    
    parser.add_argument("--p0", type=float, help="Page 0 average cycle count")
    parser.add_argument("--p1", type=float, help="Page 1 average cycle count")
    parser.add_argument("--std0", type=float, help="Page 0 cycle standard deviation")
    parser.add_argument("--std1", type=float, help="Page 1 cycle standard deviation")

    parser.add_argument("--arr0", nargs=10, type=float, help="Page 0 cycles")
    parser.add_argument("--arr1", nargs=10, type=float, help="Page 1 cycles")
    parser.add_argument("--diff", action="store_true", help="Plot difference")

    parser.add_argument("--title", type=str, required=True, help="Plot title")
    parser.add_argument("--out", type=str, required=True, help="Output filename")

    return parser.parse_args()

def main():
    args = parse_args()
    
    if args.diff:
        diff_title = args.title + " - Differences"
        diff_out = "diff_" + args.out
        plot_diff(args.arr0, args.arr1, diff_title, diff_out)
    
    avg_title = args.title + " - Average"
    plot(args.p0, args.p1, args.std0, args.std1, avg_title, args.out)

if __name__ == "__main__":
    main()
