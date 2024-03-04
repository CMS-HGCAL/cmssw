import os
import fileinput

def update_config(config_path, values_to_change):
    
    for line in fileinput.FileInput(config_path, inplace=True):
        line = line.rstrip()
        if not line:
            continue
        
        for keyword, value in values_to_change.items():
            if keyword in line:
                line = line.split("=")
                line[-1] = f"={value}"
                line = "".join(line)
        print(line)
        
    fileinput.close()

def main():
    config_path = "RecoLocalCalo/HGCalRecAlgos/test/testHGCalRecHitProducer.py"
    output_path = "calibration_output.txt"

    results = {}

    values_to_test = (1, 2, 4, 8, 16, 32, 64, 128, 256, 1024, 2048, 3584, 4096)
    # values_to_test = (16, 256)

    for n_blocks in values_to_test:
        for n_threads in values_to_test:
    
            to_change = {
                "n_blocks_value=": n_blocks,
                "n_threads_value=": n_threads,
            }
    
            update_config(config_path, to_change)
            
            command = f"cmsRun {config_path} > {output_path}"
            os.system(command)
            
            for line in fileinput.input(output_path, inplace=False):
                if "Time:" in line:
                    time = float(line.split(":")[-1])
                    break
            
            print(f"time: {time}")
            
            results[(n_blocks, n_threads)] = time
    
    print(f"{results=}")
    
    f = open("results.txt","w")
    f.write(str(results))
    f.close()
    


if __name__ == "__main__":
    main()