#include "Base.h"

namespace NoiseLib::Base
{
    float sumOfGeometricSeries(float firstTerm, float commonRatio, int numberOfTerms)
    {
        if (commonRatio == 1.0f) return firstTerm * numberOfTerms;
        return firstTerm * (1.0f - powf(commonRatio, numberOfTerms)) / (1.0f - commonRatio);
    }
}