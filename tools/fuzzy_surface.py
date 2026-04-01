from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt

OUTPUT_DIR = Path(__file__).resolve().parent.parent / "docs" / "assets"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)


def left_shoulder(x, start, end):
    if x <= start:
        return 1.0
    if x >= end:
        return 0.0
    return (end - x) / (end - start)


def triangle(x, left, center, right):
    if x <= left or x >= right:
        return 0.0
    if x == center:
        return 1.0
    if x < center:
        return (x - left) / (center - left)
    return (right - x) / (right - center)


def right_shoulder(x, start, end):
    if x <= start:
        return 0.0
    if x >= end:
        return 1.0
    return (x - start) / (end - start)


def gas_sets(x):
    return [
        left_shoulder(x, 0.0, 3.5),
        triangle(x, 0.0, 3.5, 5.0),
        right_shoulder(x, 3.5, 5.0),
    ]


def temp_sets(x):
    return [
        left_shoulder(x, -10.0, 30.0),
        triangle(x, -10.0, 30.0, 80.0),
        right_shoulder(x, 30.0, 80.0),
    ]


def output_membership(label, z):
    if label == 0:
        return left_shoulder(z, 0.0, 25.0)
    if label == 1:
        return triangle(z, 0.0, 25.0, 50.0)
    if label == 2:
        return triangle(z, 25.0, 50.0, 75.0)
    if label == 3:
        return triangle(z, 50.0, 75.0, 100.0)
    if label == 4:
        return right_shoulder(z, 75.0, 100.0)
    return 0.0


RULES = np.array([
    [0, 2, 3],
    [1, 2, 3],
    [2, 3, 4],
], dtype=int)


def evaluate_fire_score(gas_value, temp_value):
    gas_mu = gas_sets(gas_value)
    temp_mu = temp_sets(temp_value)

    output_strength = np.zeros(5, dtype=float)
    for t_idx in range(3):
        for g_idx in range(3):
            activation = min(temp_mu[t_idx], gas_mu[g_idx])
            label = RULES[t_idx, g_idx]
            output_strength[label] = max(output_strength[label], activation)

    z_values = np.arange(0.0, 101.0, 1.0)
    aggregated = np.zeros_like(z_values)

    for index, z in enumerate(z_values):
        value = 0.0
        for label, strength in enumerate(output_strength):
            value = max(value, min(strength, output_membership(label, z)))
        aggregated[index] = value

    denominator = aggregated.sum()
    if denominator == 0:
        return 0.0

    return float((z_values * aggregated).sum() / denominator)


def plot_memberships():
    gas_x = np.linspace(0.0, 5.0, 400)
    temp_x = np.linspace(-10.0, 80.0, 400)
    out_x = np.linspace(0.0, 100.0, 400)

    fig = plt.figure(figsize=(12, 4))

    ax1 = fig.add_subplot(1, 3, 1)
    ax1.plot(gas_x, [gas_sets(x)[0] for x in gas_x], label="LG")
    ax1.plot(gas_x, [gas_sets(x)[1] for x in gas_x], label="MG")
    ax1.plot(gas_x, [gas_sets(x)[2] for x in gas_x], label="HG")
    ax1.set_title("MQ-2 Memberships")
    ax1.set_xlabel("Gas level")
    ax1.set_ylabel("Membership")
    ax1.legend()

    ax2 = fig.add_subplot(1, 3, 2)
    ax2.plot(temp_x, [temp_sets(x)[0] for x in temp_x], label="LT")
    ax2.plot(temp_x, [temp_sets(x)[1] for x in temp_x], label="MT")
    ax2.plot(temp_x, [temp_sets(x)[2] for x in temp_x], label="HT")
    ax2.set_title("Temperature Memberships")
    ax2.set_xlabel("Temperature (C)")
    ax2.legend()

    ax3 = fig.add_subplot(1, 3, 3)
    for idx, name in enumerate(["VL", "L", "M", "H", "VH"]):
        ax3.plot(out_x, [output_membership(idx, x) for x in out_x], label=name)
    ax3.set_title("Output Memberships")
    ax3.set_xlabel("Fire score")
    ax3.legend()

    fig.tight_layout()
    fig.savefig(OUTPUT_DIR / "fuzzy_memberships.png", dpi=200)
    plt.close(fig)


def plot_surface():
    gas_range = np.linspace(0.0, 5.0, 80)
    temp_range = np.linspace(-10.0, 80.0, 80)
    gas_grid, temp_grid = np.meshgrid(gas_range, temp_range)

    score_grid = np.zeros_like(gas_grid)
    for i in range(score_grid.shape[0]):
        for j in range(score_grid.shape[1]):
            score_grid[i, j] = evaluate_fire_score(gas_grid[i, j], temp_grid[i, j])

    fig = plt.figure(figsize=(8, 6))
    ax = fig.add_subplot(111, projection="3d")
    ax.plot_surface(gas_grid, temp_grid, score_grid, linewidth=0, antialiased=True)
    ax.set_xlabel("MQ-2 level")
    ax.set_ylabel("Temperature (C)")
    ax.set_zlabel("Fire score")
    ax.set_title("Fuzzy Fire Surface")

    fig.tight_layout()
    fig.savefig(OUTPUT_DIR / "fuzzy_surface.png", dpi=200)
    plt.close(fig)


if __name__ == "__main__":
    plot_memberships()
    plot_surface()

    examples = [
        (1.40, 22.0),
        (2.50, 35.0),
        (4.45, 30.2),
    ]
    for gas_value, temp_value in examples:
        score = evaluate_fire_score(gas_value, temp_value)
        print(f"gas={gas_value:.2f}, temp={temp_value:.2f} -> score={score:.2f}")
