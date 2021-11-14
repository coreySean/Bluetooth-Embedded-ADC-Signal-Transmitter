
//Probably we should delete debug.h from final build
//put anything that should be debug only here.
//when we remove this header from final build this will ensure nothing points here!

/*
Guide to string formatting for dynamic logging
printf("mytext is %s", "mytext"); == "mytest is mytext"
% is the placeholder, what follows is the type of data
%d, %i = int. %i may interperet as hex or octal
%u = decimal unsigned int
%f, %F = double in fixed point
%e, %E = double in standard form
%g, %G = double normal or exponential notation
%x, %X = unsigned int as hex, changes output lower or uppercase
%0 = unsigned int octal
%s = null terminated string
%c = char (character)
%p = pointer to void
%a, %A = double in hexadecimal notation
%n = print nothing, write number of characters so far into an integer pointer parameter
//
*/
#if debug
#define debug true
//print info of this chip
void printChipSpecifications(){
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
                CONFIG_IDF_TARGET,
                chip_info.cores,
                (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

        printf("silicon revision %d, ", chip_info.revision);

        printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
                (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

        printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());
}

#endif

void restartESP(uint8_t delay){
        for (uint8_t i = 0; i <= delay; i++) {
                printf("Restarting in %d seconds...\n", i);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        fflush(stdout);
        esp_restart();
}