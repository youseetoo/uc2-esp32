import json
import matplotlib.pyplot as plt

# Raw JSON text input
raw_input = """{"encoderTable":{"axis":1,"stepSize":500,"points":20,"speed":2000,"ok":1,"data":[[0,0],[500,-76],[1000,-155],[1500,-234],[2000,-314],[2500,-394],[3000,-475],[3500,-556],[4000,-636],[4500,-715],[5000,-795],[5500,-874],[6000,-954],[6500,-1035],[7000,-1114],[7500,-1194],[8000,-1274],[8500,-1353],[9000,-1432],[9500,-1513],[10000,-1593],[9500,-1518],[9000,-1437],[8500,-1359],[8000,-1279],[7500,-1200],[7000,-1121],[6500,-1041],[6000,-960],[5500,-880],[5000,-801],[4500,-723],[4000,-643],[3500,-562],[3000,-481],[2500,-400],[2000,-321],[1500,-242],[1000,-162],[500,-83],[0,-3]]}}"""
raw_input = """{"axisCalibration":{"axis":1,"ok":1,"countsPerStep":0.3198547, "PerCount":1,"backlashCounts":4,"backlashSteps":12,"residualScatter":5,"quality":100,"microsteps":16,"r2":0.99990398990551965,"slope":0.3198545454545455,"intercept":164,"slopeForward":0.31981818181818183,"slopeReverse":0.31989090909090911,"interceptForward":164,"interceptReverse":168.34545454545452,"r2Forward":0.99994105220648,"r2Reverse":0.99990398990551965,"nForward":11,"points":[[0,158],[500,325],[1000,492],[1500,647],[2000,802],[2500,959],[3000,1121],[3500,1282],[4000,1446],[4500,1606],[5000,1761],[4500,1609],[4000,1453],[3500,1283],[3000,1124],[2500,968],[2000,804],[1500,650],[1000,496],[500,332],[0,162]],"fault":"NONE"}}"""
# Parse the JSON string
data_dict = json.loads(raw_input)
encoder_info = data_dict["axisCalibration"]
points = encoder_info["points"]

# Extract X and Y coordinates
x_vals = [pt[0] for pt in points]
y_vals = [pt[1] for pt in points]

# Separate forward (0 -> 10000) and reverse (10000 -> 0) paths
# The first 21 points go up; the remaining 20 points go back down
forward_x, forward_y = x_vals[:len(x_vals)//2], y_vals[:len(x_vals)//2]
reverse_x, reverse_y = x_vals[len(x_vals)//2-1:], y_vals[len(x_vals)//2-1:]

# Create Plot
plt.figure(figsize=(9, 5.5))
plt.plot(forward_x, forward_y, "o-", label="Forward Pass (0 → 10000)", linewidth=1.5, markersize=5)
plt.plot(reverse_x, reverse_y, "s--", label="Reverse Pass (10000 → 0)", linewidth=1.5, markersize=5)

# Formatting
plt.title(f"Encoder Table Plot (Axis {encoder_info['axis']})", fontsize=14, fontweight="bold", pad=12)
plt.xlabel("Commanded Position", fontsize=11)
plt.ylabel("Encoder Position / Offset", fontsize=11)
plt.grid(True, linestyle="--", alpha=0.6)
plt.legend(frameon=True, facecolor="white", framealpha=0.9)
plt.tight_layout()

# Show plot
plt.show()