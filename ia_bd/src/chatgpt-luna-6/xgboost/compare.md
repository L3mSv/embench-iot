# Comparação xgboost: chatgpt-luna-6 vs original

| build | escala | original | IA | iguais |
|---|---|---|---|---|
| normal | 1 | result=126 verify=1 | result=126 verify=1 | S |
| normal | 2 | result=252 verify=1 | result=252 verify=1 | S |
| normal | 4 | result=504 verify=1 | result=504 verify=1 | S |
| sanitizers | 1 | result=126 verify=1 | result=126 verify=1 | S |
| sanitizers | 2 | result=252 verify=1 | result=252 verify=1 | S |
| sanitizers | 4 | result=504 verify=1 | result=504 verify=1 | S |

**Resultado geral: IGUAIS**

## Diff do código
```diff
--- original/xgboost.c
+++ chatgpt-luna-6/xgboost.c
@@ -21,50 +21,57 @@
 
 // Run inference using XGBoost
 uint8_t predict(const uint8_t *x) {
-    uint16_t votes[NUM_CLASSES] = { 0 };
-
-    size_t tree_idx = 0;
-    size_t node_base = 0;
-    size_t leaf_base = 0;
-    for (size_t i = 0; i < NUM_CLASSES; i++) {
-        for (size_t j = 0; j < NUM_TREES; j++) {
-            const uint8_t tree_size = tree_sizes[tree_idx];
-            const uint8_t *tree_idxs = &comparison_idxs[node_base];
-            const uint8_t *tree_values = &comparison_values[node_base];
-            const uint8_t *tree_left_children = &left_children[node_base];
-            const uint8_t *tree_right_children = &right_children[node_base];
-            const uint8_t *tree_leaf_values = &leaf_values[leaf_base];
-
-            // Find leaf node for this tree
-            uint8_t node_id = 0;
-            while(!(node_id & 0x80)) { // Leaf nodes have ID >= 128
-                const uint16_t node_idx = tree_idxs[node_id];
-                const uint8_t node_value = tree_values[node_id];
-                if (x[node_idx] < node_value) { // Check condition for node
-                    node_id = tree_left_children[node_id];
-                } else {
-                    node_id = tree_right_children[node_id];
-                }
-            }
-            uint8_t leaf_idx = node_id & 0x7F; // Clear MSB to get leaf index
-            uint8_t leaf_value = tree_leaf_values[leaf_idx];
-            votes[i] += leaf_value;
-
-            tree_idx++;
-            node_base += tree_size;
-            leaf_base += tree_size+1; // n internal nodes => n+1 leaves
+    const uint8_t *sizes = tree_sizes;
+    const uint8_t *idxs = comparison_idxs;
+    const uint8_t *values = comparison_values;
+    const uint8_t *left = left_children;
+    const uint8_t *right = right_children;
+    const uint8_t *leaves = leaf_values;
+    uint16_t max_votes = 0;
+    uint8_t class_idx = 0;
+    unsigned int j;
+
+#define ACCUMULATE_TREE()                                             \
+    do {                                                              \
+        const uint8_t tree_size = *sizes++;                           \
+        uint8_t node_id = 0;                                          \
+        while (!(node_id & 0x80)) {                                   \
+            const uint8_t node_idx = idxs[node_id];                   \
+            const uint8_t node_value = values[node_id];               \
+            node_id = x[node_idx] < node_value                        \
+                ? left[node_id] : right[node_id];                     \
+        }                                                             \
+        vote += leaves[node_id & 0x7f];                               \
+        idxs += tree_size;                                            \
+        values += tree_size;                                          \
+        left += tree_size;                                            \
+        right += tree_size;                                           \
+        leaves += tree_size + 1;                                      \
+    } while (0)
+
+    for (unsigned int i = 0; i < NUM_CLASSES; ++i) {
+        uint16_t vote = 0;
+
+        /* Four independent trees per loop reduce loop control and expose
+           independent traversals for scheduling; the tail handles any
+           NUM_TREES value not divisible by four. */
+        for (j = 0; j + 3 < NUM_TREES; j += 4) {
+            ACCUMULATE_TREE();
+            ACCUMULATE_TREE();
+            ACCUMULATE_TREE();
+            ACCUMULATE_TREE();
+        }
+        for (; j < NUM_TREES; ++j)
+            ACCUMULATE_TREE();
+
+        // Strict comparison preserves the original first-class tie break.
+        if (vote > max_votes) {
+            class_idx = (uint8_t) i;
+            max_votes = vote;
         }
     }
 
-    // return argmax of votes
-    uint8_t class_idx = 0;
-    uint16_t max_votes = votes[0];
-    for (uint8_t i = 1; i < 10; i++) {
-        if (votes[i] > max_votes) {
-            class_idx = i;
-            max_votes = votes[i];
-        }
-    }
+#undef ACCUMULATE_TREE
 
     return class_idx;
 }
```