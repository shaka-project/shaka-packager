// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Combines classes from javac.custom.classpath property and ${out.dir}/classes
 * into a single jar file ${ant.project.name}.jar and places the file in
 * ${lib.java.dir}.
 */

importClass(java.io.File);
importClass(org.apache.tools.ant.types.Reference);
importClass(org.apache.tools.ant.types.FileSet);
importClass(org.apache.tools.ant.types.ZipFileSet);
importClass(org.apache.tools.ant.taskdefs.Zip);

var jarTask = project.createTask("jar");

// Do not allow duplicates in the jar, the default behavior of Jar task
// is "add" which means duplicates are allowed.
// This can cause a class file to be included multiple times, setting the
// duplicate to "preserve" ensures that only the first definition is included.

var duplicate = Zip.Duplicate();
duplicate.setValue("preserve");
jarTask.setDuplicate(duplicate);

var destPath = File(project.getProperty("TEST_JAR_PATH"));
jarTask.setDestFile(destPath);

// Include all the jars in the classpath.
var javacCustomClasspath =
    project.getReference("javac.custom.classpath").list();

for (var i in javacCustomClasspath) {
  var fileName = javacCustomClasspath[i]
  var fileExtension = fileName.split("\\.").pop();
  if(fileExtension == "jar")
  {
    var zipFileSet = ZipFileSet();
    zipFileSet.setIncludes("**/*.class");
    zipFileSet.setSrc(File(fileName));
    jarTask.addFileset(zipFileSet);
  }
}

// Add the compiled classes in ${out.dir}/classes.
var projectClasses = FileSet();
projectClasses.setIncludes("**/*.class");
projectClasses.setDir(File(project.getProperty("out.dir") + "/classes"));
jarTask.addFileset(projectClasses);

// Exclude manifest and resource classes.
var appPackagePath =
    (project.getProperty("project.app.package")).replace('.','/');
var excludedClasses = ["R.class", "R$*.class", "Manifest.class",
    "Manifest$*.class", "BuildConfig.class"]

var exclusionString = "";
for (var i in excludedClasses) {
  exclusionString += appPackagePath+ "/" + excludedClasses[i] + " ";
}

jarTask.setExcludes(exclusionString);
jarTask.perform();
