/*
 * MyConfigDb.hpp
 *
 *  Created on: May 15, 2024
 *      Author: Paul Abbott
 */
#pragma once

#include <vector>
#include <map>
#include <memory>

#include "LockableObject.hpp"

/**
 * Represents the contents of the database
 */
struct MyConfigDb
{
    std::map<std::string, std::string> settings; // Example settings map
};

using MyConfigDbManager = LockableObject<MyConfigDb>;

