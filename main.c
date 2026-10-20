#include "treekeeper.h"
#include <stdio.h>
#include <stdlib.h>

void displayMenu() {
    printf("\n--- TreeKeeper Menu ---\n");
    printf("1. Add a path to the map\n");
    printf("2. Log a new sapling\n");
    printf("3. Get the next urgent task (Watering & Routing)\n");
    printf("4. Exit\n");
    printf("Enter your choice: ");
}

int main() {
    printf("--- TreeKeeper System Initialized ---\n");
    
    // Initialize Systems for up to 100 zones and 100 trees
    Graph* plantationMap = createGraph(100);        
    MinHeap* maintenanceSchedule = createMinHeap(100); 
    
    int choice;
    
    while (1) {
        displayMenu();
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Exiting...\n");
            break;
        }
        
        switch (choice) {
            case 1: {
                int src, dest, weight;
                printf("Enter Source Zone, Destination Zone, and Distance (e.g., 0 1 5): ");
                scanf("%d %d %d", &src, &dest, &weight);
                addPath(plantationMap, src, dest, weight);
                printf("[SUCCESS] Path added between Zone %d and Zone %d.\n", src, dest);
                break;
            }
            case 2: {
                int id, timeToWater, locNode;
                char species[50];
                printf("Enter Tree ID: ");
                scanf("%d", &id);
                printf("Enter Species Name: ");
                scanf("%s", species);
                printf("Enter Next Watering Time (Priority Value - lower is more urgent): ");
                scanf("%d", &timeToWater);
                printf("Enter Location Zone ID: ");
                scanf("%d", &locNode);
                
                insertTree(maintenanceSchedule, id, species, timeToWater, locNode);
                break;
            }
            case 3: {
                Tree urgentTask = getNextTask(maintenanceSchedule);
                
                if (urgentTask.id != -1) {
                    printf("\n>>> URGENT TASK ALARM <<<\n");
                    printf("Water Tree ID: %d (%s) at Zone %d (Urgency Score: %d)\n", 
                            urgentTask.id, urgentTask.species, urgentTask.locationNode, urgentTask.nextWateringTime);
                            
                    // Route Volunteer from entrance (Zone 0) to task location
                    int startNode = 0; // Assuming volunteer starts at base/entrance
                    calculateOptimalRoute(plantationMap, startNode, urgentTask.locationNode);
                } else {
                    printf("\n[INFO] No pending tasks in the queue.\n");
                }
                break;
            }
            case 4: {
                printf("Exiting TreeKeeper System. Cleaning up memory...\n");
                
                // Memory Cleanup 
                free(maintenanceSchedule->array);
                free(maintenanceSchedule);
                // In a full production system, you would also traverse and free the graph's linked lists here
                free(plantationMap->array);
                free(plantationMap);
                
                return 0;
            }
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
    
    return 0;
}