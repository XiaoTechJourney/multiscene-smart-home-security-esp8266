#pragma once

class FuzzyFireController {
public:
  float evaluate(float gasLevel, float temperatureC) const;

private:
  static float gasLow(float x);
  static float gasMedium(float x);
  static float gasHigh(float x);

  static float tempLow(float x);
  static float tempMedium(float x);
  static float tempHigh(float x);

  static float outputMembership(int label, float z);
};
