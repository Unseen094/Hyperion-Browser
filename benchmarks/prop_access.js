// Property access benchmark
let obj = {x: 0};
let start = Date.now();
let count = 0;
for (let i = 0; i < 10000; i++) {
    obj.x = i;
    count = obj.x + obj.x;
}
console.log("Property access: 10000 iterations done");
