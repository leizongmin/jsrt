// Test FormData API
console.log("Testing FormData API...");

// Test basic FormData constructor
try {
  const formData1 = new FormData();
  console.log("✓ FormData constructor works");
} catch (error) {
  console.error("✗ FormData constructor error:", error.message);
}

// Test FormData methods
try {
  const formData = new FormData();
  
  // Test append()
  formData.append("name", "John");
  formData.append("age", "30");
  formData.append("city", "Tokyo");
  console.log("✓ FormData.append() works");
  
  // Test has()
  console.log("Has 'name':", formData.has("name"));
  console.log("Has 'email':", formData.has("email"));
  console.log("✓ FormData.has() works");
  
  // Test get()
  console.log("Get 'name':", formData.get("name"));
  console.log("Get 'nonexistent':", formData.get("nonexistent"));
  console.log("✓ FormData.get() works");
  
  // Test multiple values with same name
  formData.append("hobby", "reading");
  formData.append("hobby", "coding");
  
  // Test getAll()
  const hobbies = formData.getAll("hobby");
  console.log("All hobbies:", hobbies);
  console.log("✓ FormData.getAll() works");
  
  // Test set() - should replace existing values
  formData.set("name", "Jane");
  console.log("Name after set:", formData.get("name"));
  console.log("✓ FormData.set() works");
  
  // Test delete()
  formData.delete("age");
  console.log("Has 'age' after delete:", formData.has("age"));
  console.log("✓ FormData.delete() works");
  
  // Test forEach()
  console.log("FormData contents via forEach:");
  formData.forEach((value, key) => {
    console.log(`  ${key}: ${value}`);
  });
  console.log("✓ FormData.forEach() works");
  
} catch (error) {
  console.error("✗ FormData methods error:", error.message);
}

// Test FormData with Blob values
try {
  const formData = new FormData();
  const blob = new Blob(["file content"], {type: "text/plain"});
  
  formData.append("file", blob, "test.txt");
  console.log("✓ FormData with Blob works");
  
  const fileValue = formData.get("file");
  console.log("File value type:", typeof fileValue);
  console.log("File value size:", fileValue.size);
  
} catch (error) {
  console.error("✗ FormData with Blob error:", error.message);
}