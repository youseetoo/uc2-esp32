import re
import matplotlib.pyplot as plt
import numpy as np
def plot_values(log_text):
    """
    Extracts all 'plot:' values from log_text and plots them.
    
    Parameters
    ----------
    log_text : str
        Multiline string with logs containing 'plot: <value>'
    """
    # Extract numbers after 'plot:'
    values = [float(m) for m in re.findall(r'plot:\s*([-]?\d+\.\d+)', log_text)]
    
    if not values:
        print("No values found in logs.")
        return
    
    # Plot
    plt.figure(figsize=(12,5))
    plt.plot(values, marker='o', linestyle='-', markersize=2)
    plt.title("Encoder values from log")
    plt.xlabel("Sample index")
    plt.ylabel("Value")
    plt.grid(True, alpha=0.3)
    plt.show()
    
    return values

# Example usage:
with open("logs.txt") as f:
     log_text = f.read()
values = plot_values(log_text)
