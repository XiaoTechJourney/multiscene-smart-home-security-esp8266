#include "fuzzy_fire_controller.h"

namespace {

float clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

float leftShoulder(float x, float start, float end) {
  if (x <= start) {
    return 1.0f;
  }
  if (x >= end) {
    return 0.0f;
  }
  return clamp01((end - x) / (end - start));
}

float triangle(float x, float left, float center, float right) {
  if (x <= left || x >= right) {
    return 0.0f;
  }
  if (x == center) {
    return 1.0f;
  }
  if (x < center) {
    return clamp01((x - left) / (center - left));
  }
  return clamp01((right - x) / (right - center));
}

float rightShoulder(float x, float start, float end) {
  if (x <= start) {
    return 0.0f;
  }
  if (x >= end) {
    return 1.0f;
  }
  return clamp01((x - start) / (end - start));
}

float minValue(float a, float b) {
  return (a < b) ? a : b;
}

float maxValue(float a, float b) {
  return (a > b) ? a : b;
}

}  // namespace

float FuzzyFireController::gasLow(float x) {
  return leftShoulder(x, 0.0f, 3.5f);
}

float FuzzyFireController::gasMedium(float x) {
  return triangle(x, 0.0f, 3.5f, 5.0f);
}

float FuzzyFireController::gasHigh(float x) {
  return rightShoulder(x, 3.5f, 5.0f);
}

float FuzzyFireController::tempLow(float x) {
  return leftShoulder(x, -10.0f, 30.0f);
}

float FuzzyFireController::tempMedium(float x) {
  return triangle(x, -10.0f, 30.0f, 80.0f);
}

float FuzzyFireController::tempHigh(float x) {
  return rightShoulder(x, 30.0f, 80.0f);
}

float FuzzyFireController::outputMembership(int label, float z) {
  switch (label) {
    case 0:  // VL
      return leftShoulder(z, 0.0f, 25.0f);
    case 1:  // L
      return triangle(z, 0.0f, 25.0f, 50.0f);
    case 2:  // M
      return triangle(z, 25.0f, 50.0f, 75.0f);
    case 3:  // H
      return triangle(z, 50.0f, 75.0f, 100.0f);
    case 4:  // VH
      return rightShoulder(z, 75.0f, 100.0f);
    default:
      return 0.0f;
  }
}

float FuzzyFireController::evaluate(float gasLevel, float temperatureC) const {
  const float gasSet[3] = {
      gasLow(gasLevel),
      gasMedium(gasLevel),
      gasHigh(gasLevel),
  };

  const float tempSet[3] = {
      tempLow(temperatureC),
      tempMedium(temperatureC),
      tempHigh(temperatureC),
  };

  // row = temperature set, col = gas set
  static const int kRules[3][3] = {
      {0, 2, 3},
      {1, 2, 3},
      {2, 3, 4},
  };

  float outputStrength[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  for (int tempIndex = 0; tempIndex < 3; ++tempIndex) {
    for (int gasIndex = 0; gasIndex < 3; ++gasIndex) {
      const float activation = minValue(tempSet[tempIndex], gasSet[gasIndex]);
      const int outputLabel = kRules[tempIndex][gasIndex];
      outputStrength[outputLabel] = maxValue(outputStrength[outputLabel], activation);
    }
  }

  float numerator = 0.0f;
  float denominator = 0.0f;

  for (int z = 0; z <= 100; ++z) {
    float aggregated = 0.0f;
    for (int label = 0; label < 5; ++label) {
      const float clipped = minValue(outputStrength[label], outputMembership(label, static_cast<float>(z)));
      aggregated = maxValue(aggregated, clipped);
    }
    numerator += static_cast<float>(z) * aggregated;
    denominator += aggregated;
  }

  if (denominator <= 0.0f) {
    return 0.0f;
  }

  return numerator / denominator;
}
