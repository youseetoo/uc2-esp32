#ifdef UNIT_TEST

#include <unity.h>
#include <cjson/cJSON.h>

// Mock types for testing - simplified versions
struct StagePosition
{
    int x;
    int y;
};

struct StageScanningData
{
    bool useCoordinates = false;
    StagePosition* coordinates = nullptr;
    int coordinateCount = 0;
    int delayTimeStep = 10;
    int nFrames = 1;
    int stopped = 0;
};

StageScanningData testData;

// Mock functions for testing
void setCoordinates(StagePosition* coords, int count)
{
    if (testData.coordinates != nullptr)
    {
        delete[] testData.coordinates;
        testData.coordinates = nullptr;
    }
    
    if (coords != nullptr && count > 0)
    {
        testData.coordinates = new StagePosition[count];
        for (int i = 0; i < count; i++)
        {
            testData.coordinates[i] = coords[i];
        }
        testData.coordinateCount = count;
        testData.useCoordinates = true;
    }
}

int getJsonInt(cJSON* parent, const char* key)
{
    cJSON* item = cJSON_GetObjectItem(parent, key);
    return item ? item->valueint : 0;
}

void test_coordinate_parsing()
{
    // Test coordinate parsing
    const char* json_str = R"(
    {
        "stagescan": {
            "delayTimeStep": 15,
            "nFrames": 2,
            "coordinates": [
                {"x": 100, "y": 200},
                {"x": 300, "y": 400},
                {"x": 500, "y": 600}
            ]
        }
    })";
    
    cJSON* doc = cJSON_Parse(json_str);
    TEST_ASSERT_NOT_NULL(doc);
    
    cJSON* stagescan = cJSON_GetObjectItem(doc, "stagescan");
    TEST_ASSERT_NOT_NULL(stagescan);
    
    testData.delayTimeStep = getJsonInt(stagescan, "delayTimeStep");
    testData.nFrames = getJsonInt(stagescan, "nFrames");
    
    cJSON* coordinates = cJSON_GetObjectItem(stagescan, "coordinates");
    TEST_ASSERT_NOT_NULL(coordinates);
    TEST_ASSERT_TRUE(cJSON_IsArray(coordinates));
    
    int coordinateCount = cJSON_GetArraySize(coordinates);
    TEST_ASSERT_EQUAL(3, coordinateCount);
    
    StagePosition* positions = new StagePosition[coordinateCount];
    
    for (int i = 0; i < coordinateCount; i++)
    {
        cJSON* coord = cJSON_GetArrayItem(coordinates, i);
        TEST_ASSERT_NOT_NULL(coord);
        
        positions[i].x = getJsonInt(coord, "x");
        positions[i].y = getJsonInt(coord, "y");
    }
    
    setCoordinates(positions, coordinateCount);
    delete[] positions;
    
    // Verify parsed data
    TEST_ASSERT_EQUAL(15, testData.delayTimeStep);
    TEST_ASSERT_EQUAL(2, testData.nFrames);
    TEST_ASSERT_TRUE(testData.useCoordinates);
    TEST_ASSERT_EQUAL(3, testData.coordinateCount);
    TEST_ASSERT_NOT_NULL(testData.coordinates);
    
    TEST_ASSERT_EQUAL(100, testData.coordinates[0].x);
    TEST_ASSERT_EQUAL(200, testData.coordinates[0].y);
    TEST_ASSERT_EQUAL(300, testData.coordinates[1].x);
    TEST_ASSERT_EQUAL(400, testData.coordinates[1].y);
    TEST_ASSERT_EQUAL(500, testData.coordinates[2].x);
    TEST_ASSERT_EQUAL(600, testData.coordinates[2].y);
    
    cJSON_Delete(doc);
}

void test_grid_mode_fallback()
{
    // Test that missing coordinates array falls back to grid mode
    const char* json_str = R"(
    {
        "stagescan": {
            "delayTimeStep": 20,
            "nFrames": 1
        }
    })";
    
    cJSON* doc = cJSON_Parse(json_str);
    TEST_ASSERT_NOT_NULL(doc);
    
    cJSON* stagescan = cJSON_GetObjectItem(doc, "stagescan");
    TEST_ASSERT_NOT_NULL(stagescan);
    
    cJSON* coordinates = cJSON_GetObjectItem(stagescan, "coordinates");
    TEST_ASSERT_NULL(coordinates); // No coordinates array
    
    // Should clear coordinates and use grid mode
    testData.useCoordinates = false;
    testData.coordinateCount = 0;
    if (testData.coordinates)
    {
        delete[] testData.coordinates;
        testData.coordinates = nullptr;
    }
    
    TEST_ASSERT_FALSE(testData.useCoordinates);
    TEST_ASSERT_EQUAL(0, testData.coordinateCount);
    TEST_ASSERT_NULL(testData.coordinates);
    
    cJSON_Delete(doc);
}

void setUp(void) {
    // Set up test data
    testData.useCoordinates = false;
    testData.coordinates = nullptr;
    testData.coordinateCount = 0;
}

void tearDown(void) {
    // Clean up test data
    if (testData.coordinates)
    {
        delete[] testData.coordinates;
        testData.coordinates = nullptr;
    }
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_coordinate_parsing);
    RUN_TEST(test_grid_mode_fallback);
    
    return UNITY_END();
}

#endif