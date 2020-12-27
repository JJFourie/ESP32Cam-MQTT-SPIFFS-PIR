# PHP Server Scripts

## Upload Script
Script used by the ESP32-Cam to upload captured photo's. 
The script will:
1) capture the content uploaded from the ESP32-Cam.
2) save the uploaded photo as file "gatecam.jpg".   
    - A previously uploaded "gatecam.jpg" will be overwritten if it already exists.
    - This makes it easy for downstream functionality (e.g. Node Red) to access the latest uploaded file.
3) make a backup of the just uploaded file, using the upload timestamp to generate a unique filename.   
    
Note: if you have more than one ESP32-Cam that uploads photo's, you may want to separate the files using different scripts/directories/filenames/whatever.    
    
```
<?php

// ***************************************************************
//
// --- saveimage.php
// Save a file when uploaded from ESP32-Cam.
// - Always store new file as "gatecam.jpg".
// - Overwrite "gatecam.jpg" if it already exists.
// - Copy "gatecam.jpg" to a backup file based on the upload date and time.
//
// ***************************************************************

$workdir = "share/photos/";
$uploadFile = $workdir . "gatecam.jpg";
$backupFile = $workdir . "gatecam-" . date("ymd_Gis",  time() ) . ".jpg";

// Capture the upload content and save to file.
$received = file_get_contents('php://input');
file_put_contents($uploadFile, $received);

// Make a backup of the uploaded file (if it was created successfully)..
if( file_exists($uploadFile) ) {
        if( file_exists($backupFile) ) {
                // Backup file already exists (somehow). Generate a new filename based on current timestamp.
                sleep(1);
                $backupFile = $workdir . "gatecam-" . date("ymd_Gis",  time() ) . ".jpg";
        }
        copy ($uploadFile, $backupFile);
}

?>
```
