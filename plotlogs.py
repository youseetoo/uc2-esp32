import re
import matplotlib.pyplot as plt
import numpy as np
import argparse
import sys


def parse_logs_format(text):
    """Parse lines in format 'number; number;' and return two numpy arrays (x, y).
    
    Specifically looks for lines like: 232006; -10126927; or 233128; -100111.98;
    Supports both integer and float values.
    """
    xs = []
    ys = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        # Match format: number; number; (with trailing semicolon)
        # Updated regex to handle both integers and floats
        m = re.match(r"^\s*([-+]?(?:\d+\.?\d*|\.\d+))\s*;\s*([-+]?(?:\d+\.?\d*|\.\d+))\s*;\s*$", line)
        print(line)
        if m:
            xs.append(float(m.group(1)))
            ys.append(float(m.group(2)))
    if not xs:
        return None, None
    return np.array(xs, dtype=np.float64), np.array(ys, dtype=np.float64)


def plot_pairs(xs, ys, out_png=None, title=None):
    """Plot x vs y and optionally save to PNG; returns fig, ax."""
    # Convert x to relative values for display if values are large
    x0 = xs.min()
    x_rel = xs - x0

    fig, ax = plt.subplots(figsize=(10, 5))
    ax.plot(x_rel, ys, marker='o', linestyle='-', markersize=4)
    ax.set_xlabel('x (relative, first sample subtracted)')
    ax.set_ylabel('y')
    ax.grid(True, alpha=0.3)
    if title:
        ax.set_title(title)
    else:
        ax.set_title('Plot of semicolon-separated pairs')

    # annotate first and last points for quick inspection
    ax.annotate(f"{ys[0]:.2f}", xy=(x_rel[0], ys[0]), xytext=(5, 5), textcoords='offset points')
    ax.annotate(f"{ys[-1]:.2f}", xy=(x_rel[-1], ys[-1]), xytext=(-40, -10), textcoords='offset points')

    fig.tight_layout()
    if out_png:
        fig.savefig(out_png, dpi=200)
        print(f"Saved plot to {out_png}")
    plt.show()
    return fig, ax


def main():
    parser = argparse.ArgumentParser(description='Plot values from logs.txt in format "number; number;"')
    parser.add_argument('--out', '-o', help='Optional output PNG filename')
    args = parser.parse_args()
    
    # Always read from logs.txt
    filename = "logs.txt"
    try:
        with open(filename, 'r') as fh:
            text = fh.read()
    except Exception as e:
        print(f"Cannot open file {filename}: {e}")
        sys.exit(1)

    # Parse the specific format
    xs, ys = parse_logs_format(text)

    if xs is None:
        print('No valid pairs found in format "number; number;".')
        sys.exit(1)

    title = f"Log values plot ({len(xs)} samples)"
    plot_pairs(xs, ys, out_png=args.out, title=title)


if __name__ == '__main__':
    main()
