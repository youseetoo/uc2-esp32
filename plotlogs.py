import re
import matplotlib.pyplot as plt

def plot_last_digits(log_text, divisor=1000):
    """
    Extracts all 'plot:' values from log_text and plots their last digits
    (value % divisor). By default divisor=1000 to look at last 3 digits.
    
    Parameters
    ----------
    log_text : str
        Multiline string with logs containing 'plot: <value>'
    divisor : int, optional
        The modulus used to extract the "last digits". Default is 1000.
    """
    # Extract numbers after 'plot:'
    values = [float(m) for m in re.findall(r'plot:\s*([-]?\d+\.\d+)', log_text)]
    
    if not values:
        print("No values found in logs.")
        return
    
    # Compute last digits
    last_digits = [v % divisor for v in values]
    
    # Plot
    plt.figure(figsize=(12,5))
    plt.plot(last_digits, marker='o', linestyle='-', markersize=2)
    plt.title(f"Last digits of encoder values (mod {divisor})")
    plt.xlabel("Sample index")
    plt.ylabel(f"Value % {divisor}")
    plt.grid(True, alpha=0.3)
    plt.show()
    
    return values, last_digits


# Example usage with your log text:
with open("logs.txt") as f:
     log_text = f.read()
values, last_digits = plot_last_digits(log_text, divisor=1000)
