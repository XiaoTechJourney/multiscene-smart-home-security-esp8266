<a id="top"></a>

<div align="center">
  <h1>🔥 Fuzzy Fire Controller</h1>
  <p><strong>Design Notes and Implementation Corrections</strong></p>
  <p>Bilingual reference for the fuzzy fire-risk model used in the sensor node.</p>
  <p>
    <a href="#jp"><kbd>🇯🇵 日本語版</kbd></a>
    <a href="#en"><kbd>🇺🇸 English Version</kbd></a>
  </p>
</div>

---

<a id="jp"></a>

## 🇯🇵 日本語版

[<kbd>⬇️ English へ移動</kbd>](#en)

## 1. 設計目標

本システムでは **MQ-2 の可燃性ガス濃度** と **DHT11 の温度** を入力とし、`0 ~ 100` の火災リスク指数を出力します。  
単一しきい値だけに頼る方法より誤報を減らすことが目的です。

## 2. 入力 / 出力のファジィ集合

### 2.1 有害ガス（0 ~ 5）

- `LG`: Low Gas
- `MG`: Medium Gas
- `HG`: High Gas

対応するメンバーシップ関数:

- `LG`: 左肩型、区間 `[0, 3.5]`
- `MG`: 三角型、頂点 `3.5`
- `HG`: 右肩型、区間 `[3.5, 5]`

### 2.2 温度（-10 ~ 80 ℃）

- `LT`: Low Temperature
- `MT`: Medium Temperature
- `HT`: High Temperature

対応するメンバーシップ関数:

- `LT`: 左肩型、区間 `[-10, 30]`
- `MT`: 三角型、頂点 `30`
- `HT`: 右肩型、区間 `[30, 80]`

### 2.3 出力火災指数（0 ~ 100）

- `VL`: Very Low
- `L`: Low
- `M`: Medium
- `H`: High
- `VH`: Very High

対応する出力メンバーシップ関数:

- `VL`: 左肩型 `[0, 25]`
- `L`: 三角型 `0 / 25 / 50`
- `M`: 三角型 `25 / 50 / 75`
- `H`: 三角型 `50 / 75 / 100`
- `VH`: 右肩型 `[75, 100]`

## 3. ルール表

論文中のルール表に従い、**温度を行、ガス濃度を列** とするルール行列を使用します。

| 温度 \ ガス | LG | MG | HG |
| --- | ---: | ---: | ---: |
| LT | VL | M | H |
| MT | L | M | H |
| HT | M | H | VH |

コード上では次のように対応しています。

```cpp
// row = temperature set, col = gas set
static const int kRules[3][3] = {
  {0, 2, 3},
  {1, 2, 3},
  {2, 3, 4}
};
```

## 4. サンプル結果

代表的な入力に対して次のような出力になります。

| MQ-2 | 温度 | 火災指数 | 説明 |
| ---: | ---: | ---: | --- |
| 1.40 | 22.0 | ≈ 34.9 | 通常時の低リスク寄り |
| 2.50 | 35.0 | ≈ 45.3 | 火災疑い、確認を促すレベル |
| 4.45 | 30.2 | ≈ 65.2 | 緊急火災レベル |


## 7. コード位置

- ヘッダ: `firmware/sensor_node/fuzzy_fire_controller.h`
- 実装: `firmware/sensor_node/fuzzy_fire_controller.cpp`
- 可視化 / シミュレーションスクリプト: `tools/fuzzy_surface.py`

[<kbd>⬇️ English Section</kbd>](#en)

---

<a id="en"></a>

## 🇺🇸 English Version

[<kbd>⬆️ Back to Japanese</kbd>](#jp) [<kbd>⬆️ Top</kbd>](#top)

## 1. Design Goal

This system uses **MQ-2 combustible gas concentration** and **DHT11 temperature** as inputs, then outputs a fire-risk score from `0 ~ 100`.  
The goal is to reduce false alarms compared with a single-threshold approach.

## 2. Fuzzy Input / Output Sets

### 2.1 Harmful Gas (0 ~ 5)

- `LG`: Low Gas
- `MG`: Medium Gas
- `HG`: High Gas

Corresponding membership functions:

- `LG`: left-shoulder function over `[0, 3.5]`
- `MG`: triangular function with peak at `3.5`
- `HG`: right-shoulder function over `[3.5, 5]`

### 2.2 Temperature (-10 ~ 80 C)

- `LT`: Low Temperature
- `MT`: Medium Temperature
- `HT`: High Temperature

Corresponding membership functions:

- `LT`: left-shoulder function over `[-10, 30]`
- `MT`: triangular function with peak at `30`
- `HT`: right-shoulder function over `[30, 80]`

### 2.3 Output Fire Index (0 ~ 100)

- `VL`: Very Low
- `L`: Low
- `M`: Medium
- `H`: High
- `VH`: Very High

Corresponding output membership functions:

- `VL`: left-shoulder function `[0, 25]`
- `L`: triangular function `0 / 25 / 50`
- `M`: triangular function `25 / 50 / 75`
- `H`: triangular function `50 / 75 / 100`
- `VH`: right-shoulder function `[75, 100]`

## 3. Rule Table

Following the thesis rule table, the rule matrix is organized with **temperature as rows and gas concentration as columns**.

| Temperature \ Gas | LG | MG | HG |
| --- | ---: | ---: | ---: |
| LT | VL | M | H |
| MT | L | M | H |
| HT | M | H | VH |

In code, that mapping becomes:

```cpp
// row = temperature set, col = gas set
static const int kRules[3][3] = {
  {0, 2, 3},
  {1, 2, 3},
  {2, 3, 4}
};
```

## 4. Example Outputs

Several representative inputs produce the following outputs.

| MQ-2 | Temperature | Fire Index | Notes |
| ---: | ---: | ---: | --- |
| 1.40 | 22.0 | ≈ 34.9 | Lower-risk normal condition |
| 2.50 | 35.0 | ≈ 45.3 | Suspected fire, prompts confirmation |
| 4.45 | 30.2 | ≈ 65.2 | Emergency fire condition |

## 7. Code Locations

- Header: `firmware/sensor_node/fuzzy_fire_controller.h`
- Implementation: `firmware/sensor_node/fuzzy_fire_controller.cpp`
- Simulation script: `tools/fuzzy_surface.py`

[<kbd>⬆️ Back to Japanese</kbd>](#jp) [<kbd>⬆️ Top</kbd>](#top)
