"""
    partially used chatGPT for some basic code generation, manually checked
"""
import math
import numpy as np
import matplotlib.pyplot as plt

def plotAtan2Gradient():
    # Generate grid
    
    x = np.linspace(0, 240, 2*240+1)
    y = np.linspace(0, 240, 2*240+1)
    X, Y = np.meshgrid(x, y)

    Z = np.arctan2(120-Y, X-120)
    Z = np.degrees(Z)
    Z = np.mod(Z, 360)

    Z = np.where(Z == 360, 0.0, Z)


    dZ_dy, dZ_dx = np.gradient(Z, y, x)
    grad_mag = np.sqrt(dZ_dx**2 + dZ_dy**2)

    grad_mag = np.where(grad_mag > 2, np.nan, grad_mag)

    fig = plt.figure(figsize=(8, 6))
    # ax = fig.add_subplot(111)
    ax = fig.add_subplot(111, projection='3d')

    # surf = ax.plot_surface(X, Y, Z)
    ax.contourf(X, Y, grad_mag, levels=240, cmap="viridis")
    # ax.quiver(X, Y, dZ_dx, dZ_dy, color="white")

    # ax.contourf(X, Y, grad_mag, levels=1000, cmap="viridis")

    ax.set_xlabel('x')
    ax.set_ylabel('y')
    # ax.set_zlabel('atan2(y, x)')

    plt.show()


def plotAtan2Gradient2():
    # Generate grid
    angle = np.linspace(0, 360, 200+1)
    angle = np.deg2rad(angle)
    radius = np.linspace(50, 120, 200+1)

    R, A = np.meshgrid(radius, angle, indexing='ij')

    X = R * np.cos(A)
    Y = R * np.sin(A)

    Z = np.arctan2(Y, X)
    Z = np.degrees(Z)
    Z = np.mod(Z, 360)

    dZ_dy, dZ_dx = np.gradient(Z, angle, radius)
    grad_mag = np.sqrt(dZ_dx**2 + dZ_dy**2)
    # grad_mag = np.where(grad_mag > 2, np.nan, grad_mag)

    fig = plt.figure(figsize=(8, 6))
    ax = fig.add_subplot(111)
    # ax = fig.add_subplot(111, projection='3d')

    # ax.contourf(X, Y, Z, levels=240, cmap="viridis")
    # ax.quiver(X, Y, dZ_dx, dZ_dy, color="white")

    ax.contourf(X, Y, grad_mag, levels=240, cmap="viridis")

    ax.set_xlabel('x')
    ax.set_ylabel('y')
    # ax.set_zlabel('atan2(y, x)')

    plt.show()



plotAtan2Gradient()
# plotAtan2Gradient2()
